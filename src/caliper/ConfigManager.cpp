// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"
#include "caliper/common/StringConverter.h"

#include "../src/common/util/format_util.h"
#include "../src/common/util/parse_util.h"

#include "../services/Services.h"

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace cali;

namespace cali
{

// defined in controllers/controllers.cpp
extern const ConfigManager::ConfigInfo* builtin_controllers_table[];

extern const char* builtin_base_option_specs;
extern const char* builtin_gotcha_option_specs;
extern const char* builtin_libdw_option_specs;
extern const char* builtin_mpi_option_specs;
extern const char* builtin_openmp_option_specs;
extern const char* builtin_cuda_option_specs;
extern const char* builtin_rocm_option_specs;
extern const char* builtin_pcp_option_specs;
extern const char* builtin_umpire_option_specs;
extern const char* builtin_kokkos_option_specs;

extern const char* builtin_papi_hsw_option_specs;
extern const char* builtin_papi_skl_option_specs;
extern const char* builtin_papi_spr_option_specs;

extern void add_submodule_controllers_and_services();

} // namespace cali

namespace
{

ChannelController* make_basic_channel_controller(
    const char*                   name,
    const config_map_t&           initial_cfg,
    const ConfigManager::Options& opts
)
{
    class BasicController : public ChannelController
    {
    public:

        BasicController(const char* name, const config_map_t& initial_cfg, const ConfigManager::Options& opts)
            : ChannelController(name, 0, initial_cfg)
        {
            // Hacky way to handle "output" option
            if (opts.is_set("output")) {
                std::string output = opts.get("output");

                config()["CALI_RECORDER_FILENAME"]  = output;
                config()["CALI_REPORT_FILENAME"]    = output;
                config()["CALI_MPIREPORT_FILENAME"] = output;
            }

            opts.update_channel_config(config());
            opts.update_channel_metadata(metadata());
        }
    };

    return new BasicController(name, initial_cfg, opts);
}

class ConfigSpecManager
{
    std::vector<ConfigManager::ConfigInfo> m_configs;

public:

    void add_controller_specs(const ConfigManager::ConfigInfo** specs)
    {
        for (const ConfigManager::ConfigInfo** s = specs; s && *s; s++)
            m_configs.push_back(**s);
    }

    std::vector<ConfigManager::ConfigInfo> get_config_specs() const { return m_configs; }

    static ConfigSpecManager* instance()
    {
        static std::unique_ptr<ConfigSpecManager> mP { new ConfigSpecManager };

        if (mP->m_configs.empty())
            mP->add_controller_specs(builtin_controllers_table);

        return mP.get();
    }
};

ConfigManager::arglist_t merge_new_elements(ConfigManager::arglist_t& to, const ConfigManager::arglist_t& from)
{
    for (auto& p : from) {
        auto it = std::find_if(to.begin(), to.end(), [p](const std::pair<std::string, std::string>& v) {
            return p.first == v.first;
        });

        if (it == to.end() || p.first == "metadata") // hacky but we want to allow multiple entries
                                                     // for metadata
            to.push_back(p);
    }

    return to;
}

std::vector<std::string> to_stringlist(const std::vector<StringConverter>& list)
{
    std::vector<std::string> ret;
    ret.reserve(list.size());

    for (const StringConverter& sc : list)
        ret.push_back(sc.to_string());

    return ret;
}

std::string join_stringlist(const std::vector<std::string>& list)
{
    std::string ret;
    int         c = 0;

    for (const std::string& s : list) {
        if (c++ > 0)
            ret.append(",");
        ret.append(s);
    }

    return ret;
}

void join_stringlist(std::string& in, const std::vector<std::string>& list)
{
    for (const auto& s : list) {
        if (!in.empty())
            in.append(",");
        in.append(s);
    }
}

std::string find_or(const std::map<std::string, std::string>& m, const std::string& k, const std::string v = "")
{
    auto it = m.find(k);
    return it != m.end() ? it->second : v;
}

std::string expand_variables(const std::string& in, const std::string& val)
{
    std::string        ret;
    std::istringstream is(in);

    bool esc = false;

    while (is.good()) {
        char c = is.get();

        if (c == '\\') {
            c = is.get();
            if (is.good())
                ret.push_back(c);
            continue;
        } else if (c == '"') {
            esc = !esc;
            continue;
        } else if (c == '{') {
            c = is.get();
            if (c == '}') {
                ret.append(val);
                continue;
            } else {
                ret.push_back('{');
            }
        }

        if (!is.good())
            break;

        ret.push_back(c);
    }

    return ret;
}

ConfigManager::arglist_t parse_keyval_list(std::istream& is)
{
    ConfigManager::arglist_t ret;
    char                     c = 0;

    do {
        std::string key = util::read_word(is, "=");
        if (key.empty())
            return ret;

        c = util::read_char(is);
        if (c != '=')
            return ret;

        std::string val = util::read_word(is, ",()");
        if (!val.empty())
            ret.push_back(std::make_pair(key, val));

        c = util::read_char(is);
    } while (is.good() && c == ',');

    if (is.good())
        is.unget();

    return ret;
}

std::pair<bool, StringConverter> find_key_in_json(
    const std::vector<std::string>&               path,
    const std::map<std::string, StringConverter>& dict
)
{
    if (path.empty())
        return std::make_pair(false, StringConverter());

    auto it = dict.find(path.front());
    if (it == dict.end())
        return std::make_pair(false, StringConverter());

    if (path.size() == 1)
        return std::make_pair(true, it->second);

    StringConverter ret = it->second;
    for (auto path_it = path.begin() + 1; path_it != path.end(); ++path_it) {
        auto sub_dict = ret.rec_dict();
        it            = sub_dict.find(*path_it);
        if (it == sub_dict.end())
            return std::make_pair(false, StringConverter());
        ret = it->second;
    }

    return std::make_pair(true, ret);
}

unsigned add_metadata_entries(const std::string& key, const StringConverter& val, info_map_t& info)
{
    unsigned num_entries = 0;

    bool is_dict = false;
    auto dict    = val.rec_dict(&is_dict);

    if (is_dict) {
        std::string prefix = key + ".";
        for (const auto& p : dict)
            num_entries += add_metadata_entries(prefix + p.first, p.second, info);
    } else {
        info[key] = val.to_string();
        ++num_entries;
    }

    return num_entries;
}

void read_metadata_from_json_file(const std::string& filename, const std::string& keys, info_map_t& info)
{
    std::ifstream is(filename.c_str(), std::ios::ate);

    if (!is) {
        Log(0).stream() << "read_metadata_from_json_file(): Cannot open file " << filename << ", quitting\n";
        return;
    }

    auto        size = is.tellg();
    std::string str(size, '\0');
    is.seekg(0);
    if (!is.read(&str[0], size)) {
        Log(0).stream() << "read_metadata_from_json_file(): Cannot read file " << filename << ", quitting\n";
        return;
    }

    bool ok  = false;
    auto top = StringConverter(str).rec_dict(&ok);

    if (!ok) {
        Log(0).stream() << "read_metadata_from_json_file(): Cannot parse top-level dict in " << filename
                        << ", quitting\n";
        return;
    }

    auto keylist = StringConverter(keys).to_stringlist();
    if (keylist.empty()) {
        for (const auto& p : top)
            keylist.push_back(p.first);
    }

    for (const std::string& key : keylist) {
        std::vector<std::string> path = StringConverter(key).to_stringlist(".");
        auto                     ret  = find_key_in_json(path, top);
        if (ret.first)
            add_metadata_entries(key, ret.second, info);
        else
            Log(1).stream() << "read_metadata_from_json_file(): Key " << key << " not found\n";
    }
}

void add_metadata(const std::string& args, info_map_t& info)
{
    std::istringstream is(args);
    auto               arglist = parse_keyval_list(is);

    auto it = std::find_if(arglist.begin(), arglist.end(), [](const std::pair<std::string, std::string>& p) {
        return p.first == "file";
    });

    if (it != arglist.end()) {
        std::string filename = it->second;
        std::string keys;
        it = std::find_if(arglist.begin(), arglist.end(), [](const std::pair<std::string, std::string>& p) {
            return p.first == "keys";
        });
        if (it != arglist.end())
            keys = it->second;
        read_metadata_from_json_file(filename, keys, info);
    } else {
        for (const auto& p : arglist)
            info[p.first] = p.second;
    }
}

} // namespace

//
// --- OptionSpec
//

class ConfigManager::OptionSpec
{
    struct query_arg_t {
        std::vector<std::string> select;
        std::vector<std::string> groupby;
        std::vector<std::string> let;
        std::vector<std::string> where;
        std::vector<std::string> aggregate;
        std::vector<std::string> orderby;
    };

    struct option_spec_t {
        std::string type;
        std::string description;
        std::string category;

        std::vector<std::string> services;
        std::vector<std::string> inherited_specs;

        std::map<std::string, query_arg_t> query_args;
        std::map<std::string, std::string> config;
    };

    std::map<std::string, option_spec_t> data;

    bool        m_error;
    std::string m_error_msg;

    void set_error(const std::string& msg)
    {
        m_error     = true;
        m_error_msg = msg;
    }

    void parse_select(const std::vector<StringConverter>& list, query_arg_t& qarg)
    {
        for (const StringConverter& sc : list) {
            bool                                   is_a_dict = false;
            std::map<std::string, StringConverter> dict      = sc.rec_dict(&is_a_dict);

            // The deprecated syntax for a select spec is a list of
            //   { "expr": "expression", "as": "alias", "unit": "unit" }
            // dicts. The new syntax is just a list of
            //   "expression AS alias UNIT unit"
            // strings. Determine which we have and parse accordingly.
            if (is_a_dict) {
                std::string str = dict["expr"].to_string();
                auto        it  = dict.find("as");
                if (it != dict.end()) {
                    str.append(" as \"");
                    str.append(it->second.to_string());
                    str.append("\"");
                }
                it = dict.find("unit");
                if (it != dict.end()) {
                    str.append(" unit \"");
                    str.append(it->second.to_string());
                    str.append("\"");
                }
                qarg.select.push_back(str);
            } else {
                qarg.select.push_back(sc.to_string());
            }
        }
    }

    void parse_query_args(const std::vector<StringConverter>& list, option_spec_t& opt)
    {
        for (const StringConverter& sc : list) {
            std::map<std::string, StringConverter> dict = sc.rec_dict();
            query_arg_t                            qarg;

            auto it = dict.find("group by");
            if (it != dict.end())
                qarg.groupby = ::to_stringlist(it->second.rec_list());

            it = dict.find("let");
            if (it != dict.end())
                qarg.let = ::to_stringlist(it->second.rec_list());

            it = dict.find("where");
            if (it != dict.end())
                qarg.where = ::to_stringlist(it->second.rec_list());

            it = dict.find("aggregate");
            if (it != dict.end())
                qarg.aggregate = ::to_stringlist(it->second.rec_list());

            it = dict.find("order by");
            if (it != dict.end())
                qarg.orderby = ::to_stringlist(it->second.rec_list());

            it = dict.find("select");
            if (it != dict.end())
                parse_select(it->second.rec_list(), qarg);

            it = dict.find("level");
            if (it == dict.end()) {
                set_error(": query arg: missing \"level\"");
                continue;
            }

            opt.query_args[it->second.to_string()] = qarg;
        }
    }

    void parse_config(const std::map<std::string, StringConverter>& dict, option_spec_t& opt)
    {
        for (auto p : dict)
            opt.config[p.first] = p.second.to_string();
    }

    void parse_spec(const std::map<std::string, StringConverter>& dict)
    {
        option_spec_t opt;
        bool          ok = true;

        auto it = dict.find("category");
        if (it != dict.end())
            opt.category = it->second.to_string();
        it = dict.find("services");
        if (it != dict.end())
            opt.services = ::to_stringlist(it->second.rec_list(&ok));
        it = dict.find("inherit");
        if (ok && !m_error && it != dict.end())
            opt.inherited_specs = ::to_stringlist(it->second.rec_list(&ok));
        it = dict.find("config");
        if (ok && !m_error && it != dict.end())
            parse_config(it->second.rec_dict(&ok), opt);
        it = dict.find("query");
        if (ok && !m_error && it != dict.end())
            parse_query_args(it->second.rec_list(&ok), opt);
        it = dict.find("type");
        if (ok && !m_error && it != dict.end())
            opt.type = it->second.to_string();
        it = dict.find("description");
        if (ok && !m_error && it != dict.end())
            opt.description = it->second.to_string();

        it = dict.find("name");
        if (it == dict.end()) {
            set_error(": \"name\" missing");
            return;
        }

        if (!ok)
            set_error(": parse error");
        if (!m_error)
            data[it->second.to_string()] = opt;
    }

    std::vector<std::string> recursive_get_services_list(const std::string& cfg)
    {
        std::vector<std::string> ret;

        auto it = data.find(cfg);

        if (it == data.end())
            return ret;

        ret = it->second.services;

        for (const std::string& s : it->second.inherited_specs) {
            auto tmp = recursive_get_services_list(s);
            ret.insert(ret.end(), tmp.begin(), tmp.end());
        }

        std::sort(ret.begin(), ret.end());
        ret.erase(std::unique(ret.begin(), ret.end()), ret.end());

        return ret;
    }

public:

    OptionSpec() : m_error(false) {}

    OptionSpec(const OptionSpec&) = default;
    OptionSpec(OptionSpec&&)      = default;

    OptionSpec& operator= (const OptionSpec&) = default;
    OptionSpec& operator= (OptionSpec&&)      = default;

    ~OptionSpec() {}

    bool error() const { return m_error; }

    std::string error_msg() const { return m_error_msg; }

    void add(const OptionSpec& other, const std::vector<std::string>& categories)
    {
        for (const auto& p : other.data)
            if (std::find(categories.begin(), categories.end(), p.second.category) != categories.end())
                data.insert(p);
    }

    void add(const std::vector<StringConverter>& list)
    {
        if (m_error)
            return;

        for (auto& p : list) {
            parse_spec(p.rec_dict());

            if (m_error) {
                m_error_msg = std::string("option spec: ") + util::clamp_string(p.to_string(), 32) + m_error_msg;
                break;
            }
        }
    }

    bool contains(const std::string& name) const { return data.find(name) != data.end(); }

    std::map<std::string, std::string> get_option_descriptions()
    {
        std::map<std::string, std::string> ret;

        for (auto& p : data)
            ret.insert(std::make_pair(p.first, p.second.description));

        return ret;
    }

    // throw out options requiring unavailable services
    void filter_unavailable_options(const std::vector<std::string>& in)
    {
        std::vector<std::string> available(in);
        std::sort(available.begin(), available.end());
        available.erase(std::unique(available.begin(), available.end()), available.end());

        auto it = data.begin();

        while (it != data.end()) {
            auto required = recursive_get_services_list(it->first);

            if (std::includes(available.begin(), available.end(), required.begin(), required.end()))
                ++it;
            else
                it = data.erase(it);
        }
    }

    friend class ConfigManager::Options;
};

//
// --- Options
//

struct ConfigManager::Options::OptionsImpl {
    OptionSpec spec;
    arglist_t  args;

    std::vector<std::string> enabled_options;

    std::string check() const
    {
        //
        // Check if option value has the correct datatype
        //
        for (const auto& arg : args) {
            auto it = spec.data.find(arg.first);

            if (it == spec.data.end())
                continue;

            if (it->second.type == "bool") {
                bool ok = false;
                StringConverter(arg.second).to_bool(&ok);
                if (!ok)
                    return std::string("Invalid value \"") + arg.second + std::string("\" for ") + arg.first;
            }
        }

        //
        //   Check if the required services for all requested profiling options
        // are there
        //

        add_submodule_controllers_and_services();
        auto slist = services::get_available_services();

        for (const std::string& opt : enabled_options) {
            auto o_it = spec.data.find(opt);
            if (o_it == spec.data.end())
                continue;

            for (const std::string& required_service : o_it->second.services)
                if (std::find(slist.begin(), slist.end(), required_service) == slist.end())
                    return required_service + std::string(" service required for ") + o_it->first
                           + std::string(" option is not available");
        }

        return "";
    }

    std::string services(const std::string& in)
    {
        std::vector<std::string> vec = StringConverter(in).to_stringlist();

        for (const std::string& opt : enabled_options) {
            auto it = spec.data.find(opt);
            if (it == spec.data.end())
                continue;

            vec.insert(vec.end(), it->second.services.begin(), it->second.services.end());
        }

        // remove duplicates
        std::sort(vec.begin(), vec.end());
        auto last = std::unique(vec.begin(), vec.end());
        vec.erase(last, vec.end());

        return ::join_stringlist(vec);
    }

    void append_config(config_map_t& config)
    {
        for (const std::string& opt : enabled_options) {
            auto spec_it = spec.data.find(opt);
            if (spec_it == spec.data.end())
                continue;

            for (const auto& kv_p : spec_it->second.config) {
                auto it = std::find_if(args.begin(), args.end(), [&opt](const std::pair<std::string, std::string>& p) {
                    return opt == p.first;
                });
                if (it != args.end())
                    // replace "{}" variable placeholders in spec with argument, if any
                    config[kv_p.first] = ::expand_variables(kv_p.second, it->second);
            }
        }
    }

    void update_channel_config(config_map_t& config)
    {
        config["CALI_SERVICES_ENABLE"] = services(config["CALI_SERVICES_ENABLE"]);
        append_config(config);
    }

    void update_channel_metadata(info_map_t& info)
    {
        for (const auto& p : args) {
            if (p.first == "metadata") {
                ::add_metadata(p.second, info);
            } else {
                std::string key = "opts:";
                key.append(p.first);
                info[key] = p.second;
            }
        }
    }

    std::string build_query(const char* level, const std::map<std::string, std::string>& in) const
    {
        std::string q_let       = ::find_or(in, "let");
        std::string q_select    = ::find_or(in, "select");
        std::string q_groupby   = ::find_or(in, "group by");
        std::string q_where     = ::find_or(in, "where");
        std::string q_aggregate = ::find_or(in, "aggregate");
        std::string q_orderby   = ::find_or(in, "order by");
        std::string q_format    = ::find_or(in, "format");

        for (const std::string& opt : enabled_options) {
            auto s_it = spec.data.find(opt);
            if (s_it == spec.data.end())
                continue;

            auto l_it = s_it->second.query_args.find(level);
            if (l_it != s_it->second.query_args.end()) {
                const auto& q = l_it->second;
                ::join_stringlist(q_let, q.let);
                ::join_stringlist(q_select, q.select);
                ::join_stringlist(q_groupby, q.groupby);
                ::join_stringlist(q_where, q.where);
                ::join_stringlist(q_aggregate, q.aggregate);
                ::join_stringlist(q_orderby, q.orderby);
            }
        }

        std::string ret;

        if (!q_let.empty())
            ret.append(" let ").append(q_let);
        if (!q_select.empty())
            ret.append(" select ").append(q_select);
        if (!q_groupby.empty())
            ret.append(" group by ").append(q_groupby);
        if (!q_where.empty())
            ret.append(" where ").append(q_where);
        if (!q_aggregate.empty())
            ret.append(" aggregate ").append(q_aggregate);
        if (!q_orderby.empty())
            ret.append(" order by ").append(q_orderby);
        if (!q_format.empty())
            ret.append(" format ").append(q_format);

        return ret;
    }

    std::vector<std::string> get_inherited_specs(const std::string& name)
    {
        std::vector<std::string> ret;

        auto it = spec.data.find(name);
        if (it == spec.data.end())
            return ret;

        for (const std::string& inh : it->second.inherited_specs) {
            auto tmp = get_inherited_specs(inh);
            ret.insert(ret.end(), tmp.begin(), tmp.end());
            ret.push_back(inh);
        }

        return ret;
    }

    void find_enabled_options()
    {
        std::vector<std::string> vec;

        for (const auto& argp : args) {
            auto s_it = spec.data.find(argp.first);
            if (s_it == spec.data.end())
                continue;

            //   Non-boolean options are enabled if they are present in args.
            // For boolean options, check if they are set to false or true.
            bool enabled = true;
            if (s_it->second.type == "bool")
                enabled = StringConverter(argp.second).to_bool();
            if (enabled) {
                vec.push_back(argp.first);
                auto tmp = get_inherited_specs(argp.first);
                vec.insert(vec.end(), tmp.begin(), tmp.end());
            }
        }

        // make sure there are no duplicates, but keep entries in input order
        std::vector<std::string> ret;
        ret.reserve(vec.size());
        for (auto it = vec.begin(); it != vec.end(); ++it) {
            if (std::find(ret.begin(), ret.end(), *it) == ret.end())
                ret.emplace_back(std::move(*it));
        }

        enabled_options = std::move(ret);
    }

    OptionsImpl(const OptionSpec& s, const arglist_t& a) : spec(s), args(a) { find_enabled_options(); }
};

ConfigManager::Options::Options(const OptionSpec& spec, const arglist_t& args) : mP(new OptionsImpl(spec, args))
{}

ConfigManager::Options::~Options()
{
    mP.reset();
}

bool ConfigManager::Options::is_set(const char* option) const
{
    std::string ostr(option);
    return std::find_if(
               mP->args.begin(),
               mP->args.end(),
               [&ostr](const std::pair<std::string, std::string>& p) { return ostr == p.first; }
           )
           != mP->args.end();
}

bool ConfigManager::Options::is_enabled(const char* option) const
{
    return std::find(mP->enabled_options.begin(), mP->enabled_options.end(), std::string(option))
           != mP->enabled_options.end();
}

std::vector<std::string> ConfigManager::Options::enabled_options() const
{
    std::vector<std::string> ret;

    for (const auto& s : mP->enabled_options) {
        auto s_it = mP->spec.data.find(s);
        if (s_it == mP->spec.data.end())
            continue;
        if (s_it->second.type == "bool")
            ret.push_back(s);
    }

    return ret;
}

std::string ConfigManager::Options::get(const char* option, const char* default_val) const
{
    std::string ostr(option);
    auto it = std::find_if(mP->args.begin(), mP->args.end(), [&ostr](const std::pair<std::string, std::string>& p) {
        return ostr == p.first;
    });

    if (it != mP->args.end())
        return it->second;

    return std::string(default_val);
}

std::string ConfigManager::Options::check() const
{
    return mP->check();
}

void ConfigManager::Options::update_channel_config(config_map_t& config) const
{
    mP->update_channel_config(config);
}

void ConfigManager::Options::update_channel_metadata(info_map_t& metadata) const
{
    mP->update_channel_metadata(metadata);
}

std::string ConfigManager::Options::build_query(const char* level, const std::map<std::string, std::string>& in) const
{
    return mP->build_query(level, in);
}

//
// --- ConfigManager
//

struct ConfigManager::ConfigManagerImpl {
    ChannelList m_channels;

    bool        m_error     = false;
    std::string m_error_msg = "";

    std::vector<const char*> builtin_option_specs_list;

    std::map<std::string, arglist_t> m_default_parameters_for_spec;

    arglist_t m_default_parameters;
    argmap_t  m_extra_vars;

    OptionSpec m_global_opts;

    struct config_spec_t {
        std::string              json; // the json spec
        CreateConfigFn           create;
        CheckArgsFn              check_args;
        std::string              name;
        std::vector<std::string> categories;
        std::string              description;
        config_map_t             initial_cfg;
        OptionSpec               opts;
        arglist_t                defaults; // default options, if any
    };

    std::map<std::string, std::shared_ptr<config_spec_t>> m_spec;

    void set_error(const std::string& msg)
    {
        m_error     = true;
        m_error_msg = msg;
    }

    void set_error(const std::string& msg, std::istream& is)
    {
        m_error     = true;
        m_error_msg = msg;

        size_t maxctx = 16;

        if (is.good()) {
            is.unget();
            m_error_msg.append(" at ");

            for (char c = is.get(); is.good() && --maxctx > 0; c = is.get())
                m_error_msg.push_back(c);
            if (maxctx == 0)
                m_error_msg.append("...");
        }
    }

    // sets error if err is set
    void check_error(const std::string& err)
    {
        if (err.empty())
            return;

        set_error(err);
    }

    void add_config_spec(const char* jsonspec, CreateConfigFn create, CheckArgsFn check, bool ignore_existing)
    {
        config_spec_t spec;
        bool          ok = false;

        spec.json       = jsonspec;
        spec.create     = (create == nullptr) ? ::make_basic_channel_controller : create;
        spec.check_args = check;

        auto dict = StringConverter(spec.json).rec_dict(&ok);

        if (!ok) {
            set_error(std::string("spec parse error: ") + util::clamp_string(spec.json, 48));
            return;
        }

        auto it = dict.find("name");
        if (it == dict.end()) {
            set_error(std::string("'name' missing in spec: ") + util::clamp_string(spec.json, 48));
            return;
        }

        spec.name = it->second.to_string();

        // check if the spec already exists
        if (m_spec.count(spec.name) > 0) {
            if (!ignore_existing)
                set_error(spec.name + std::string(" already exists"));
            return;
        }

        std::vector<std::string> cfg_srvcs;

        it = dict.find("services");
        if (ok && !m_error && it != dict.end())
            cfg_srvcs = ::to_stringlist(it->second.rec_list(&ok));

        services::add_default_service_specs();
        auto slist = services::get_available_services();

        bool have_all_services = true;
        for (const auto& s : cfg_srvcs)
            if (std::find(slist.begin(), slist.end(), s) == slist.end())
                have_all_services = false;

        if (!have_all_services)
            return;

        it = dict.find("categories");
        if (ok && !m_error && it != dict.end())
            spec.categories = ::to_stringlist(it->second.rec_list(&ok));
        it = dict.find("description");
        if (ok && !m_error && it != dict.end())
            spec.description = it->second.to_string();
        it = dict.find("options");
        if (ok && !m_error && it != dict.end())
            spec.opts.add(it->second.rec_list(&ok));
        it = dict.find("config");
        if (ok && !m_error && it != dict.end())
            for (const auto& p : it->second.rec_dict(&ok))
                spec.initial_cfg[p.first] = p.second.to_string();

        if (cfg_srvcs.size() > 0)
            spec.initial_cfg["CALI_SERVICES_ENABLE"].append(::join_stringlist(cfg_srvcs));

        it = dict.find("defaults");
        if (ok && !m_error && it != dict.end())
            for (const auto& p : it->second.rec_dict(&ok))
                spec.defaults.push_back(std::make_pair(p.first, p.second.to_string()));
        ;

        if (!ok)
            set_error(std::string("spec parse error: ") + util::clamp_string(spec.json, 48));
        if (!m_error)
            m_spec.emplace(spec.name, std::make_shared<config_spec_t>(spec));
    }

    void add_global_option_specs(const char* json)
    {
        bool ok = false;
        m_global_opts.add(StringConverter(json).rec_list(&ok));

        if (m_global_opts.error())
            set_error(m_global_opts.error_msg());
        if (!ok)
            set_error(std::string("parse error: ") + util::clamp_string(json, 48));
    }

    void import_builtin_config_specs()
    {
        add_submodule_controllers_and_services();

        for (const ConfigInfo& s : ::ConfigSpecManager::instance()->get_config_specs())
            add_config_spec(s.spec, s.create, s.check_args, true /* ignore existing */);
    }

    //   Parse "=value" or "(value)"
    // Returns an empty string if there is no '=', otherwise the string after '='.
    // Sets error if there is a '=' but no word afterwards.
    std::string read_value(std::istream& is, const std::string& key)
    {
        std::string val;
        char        c = util::read_char(is);

        if (c == '=') {
            val = util::read_word(is, ",=()\n");

            if (val.empty())
                set_error("Expected value after \"" + key + "=\"", is);
        } else if (c == '(') {
            val = util::read_nested_text(is, '(', ')');
            c   = util::read_char(is);
            if (c != ')')
                set_error("Missing ')' after \"" + key + "\"(", is);
        } else
            is.unget();

        return val;
    }

    arglist_t parse_arglist(std::istream& is, const OptionSpec& opts)
    {
        arglist_t args;

        char c = util::read_char(is);

        if (!is.good())
            return args;

        if (c != '(') {
            is.unget();
            return args;
        }

        do {
            std::string key = util::read_word(is, ",=()\n");

            if (!key.empty()) {
                if (!(opts.contains(key)) && key != "metadata") {
                    set_error("Unknown option: " + key);
                    args.clear();
                    return args;
                }

                std::string val = read_value(is, key);

                if (m_error) {
                    args.clear();
                    return args;
                }

                if (val.empty())
                    val = "true";

                args.push_back(std::make_pair(key, val));
            }

            c = util::read_char(is);
        } while (is.good() && c == ',');

        if (c != ')') {
            set_error("Expected ')'", is);
            args.clear();
        }

        return args;
    }

    // Return true if key is an option in any config
    bool is_option(const std::string& key)
    {
        if (m_global_opts.contains(key))
            return true;

        for (const auto& p : m_spec)
            if (p.second->opts.contains(key))
                return true;

        return false;
    }

    void parse_json_content(const std::string& json)
    {
        bool ok   = false;
        auto dict = StringConverter(json).rec_dict(&ok);

        if (ok) {
            // first, try and see if this is a single config spec
            if (dict.count("name") > 0) {
                add_config_spec(json.c_str(), nullptr, nullptr, false);
                return;
            }

            // see if we have "configs" and "options" lists and parse them
            auto it = dict.find("options");
            if (it != dict.end()) {
                ok = false;
                m_global_opts.add(it->second.rec_list(&ok));

                if (m_global_opts.error())
                    set_error(m_global_opts.error_msg());
                if (!ok)
                    set_error(std::string("parse error: ") + util::clamp_string(it->second.to_string(), 48));
            }

            it = dict.find("configs");
            if (it != dict.end()) {
                auto configs = it->second.rec_list(&ok);
                if (!ok) {
                    set_error(std::string("parse error: ") + util::clamp_string(it->second.to_string(), 48));
                    return;
                }
                for (auto& s : configs) {
                    std::string buf = s.to_string();
                    add_config_spec(buf.c_str(), nullptr, nullptr, false);

                    if (m_error)
                        return;
                }
            }
        } else {
            // try to parse a list of config specs

            auto list = StringConverter(json).rec_list(&ok);

            if (ok) {
                for (auto& s : list) {
                    add_config_spec(s.to_string().c_str(), nullptr, nullptr, false);
                    if (m_error)
                        return;
                }
            } else {
                set_error(std::string("parse error: ") + util::clamp_string(json, 48));
            }
        }
    }

    // Load config and/or option specs from file
    void load_file(const std::string& filename)
    {
        if (std::ifstream is { filename, std::ios::ate }) {
            auto        size = is.tellg();
            std::string str(size, '\0');
            is.seekg(0);
            if (is.read(&str[0], size))
                parse_json_content(str);
        } else {
            set_error(std::string("Could not open file ") + filename);
        }
    }

    void handle_load_command(std::istream& is)
    {
        std::vector<std::string> ret;
        char                     c = util::read_char(is);

        if (c != '(') {
            set_error("Expected '(' after \"load\"", is);
            return;
        }

        do {
            std::string filename = util::read_word(is, ",()");

            if (!filename.empty())
                load_file(filename);
            else
                set_error("Expected filename for \"load\"", is);

            if (m_error)
                return;

            c = util::read_char(is);
        } while (is.good() && c == ',');

        if (c != ')') {
            set_error("Missing ')' after \"load(\"", is);
        }
    }

    //   Returns found configs with their args. Also updates the default parameters list
    // and extra variables list.
    std::vector<std::pair<const std::shared_ptr<config_spec_t>, arglist_t>> parse_configstring(const char* config_string
    )
    {
        import_builtin_config_specs();

        std::vector<std::pair<const std::shared_ptr<config_spec_t>, arglist_t>> ret;

        std::istringstream is(config_string);
        char               c = 0;

        //   Return if string is only whitespace.
        // Prevents empty strings being marked as errors.
        do {
            c = util::read_char(is);
        } while (is.good() && isspace(c));

        if (is.good())
            is.unget();
        else
            return ret;

        do {
            std::string key = util::read_word(is, ",=()\n");

            if (key == "load") {
                handle_load_command(is);

                if (m_error)
                    return ret;
            } else {
                auto spec_p = m_spec.find(key);
                if (spec_p != m_spec.end() && spec_p->second) {
                    OptionSpec opts(spec_p->second->opts);
                    opts.add(m_global_opts, spec_p->second->categories);

                    auto args = parse_arglist(is, opts);

                    if (m_error)
                        return ret;

                    ret.push_back(std::make_pair(spec_p->second, std::move(args)));
                } else {
                    std::string val = read_value(is, key);

                    if (m_error)
                        return ret;

                    if (key == "metadata") {
                        m_default_parameters.push_back(std::make_pair(key, val));
                    } else if (is_option(key)) {
                        if (val.empty())
                            val = "true";

                        m_default_parameters.push_back(std::make_pair(key, val));
                    } else
                        m_extra_vars[key] = val;
                }
            }

            c = util::read_char(is);
        } while (!m_error && is.good() && c == ',');

        if (!m_error && is.good()) {
            // We read something unexpected
            set_error(std::string("Unexpected \'").append(1, c).append("\'"), is);
        }

        return ret;
    }

    arglist_t add_default_parameters(arglist_t& args, const config_spec_t& spec) const
    {
        auto it = m_default_parameters_for_spec.find(spec.name);

        if (it != m_default_parameters_for_spec.end())
            merge_new_elements(args, it->second);

        merge_new_elements(args, m_default_parameters);
        merge_new_elements(args, spec.defaults);

        return args;
    }

    OptionSpec options_for_config(const config_spec_t& config) const
    {
        OptionSpec opts(config.opts);
        opts.add(m_global_opts, config.categories);
        return opts;
    }

    ChannelList parse(const char* config_string)
    {
        auto configs = parse_configstring(config_string);

        if (m_error)
            return ChannelList();

        ChannelList ret;
        ret.reserve(configs.size());

        for (auto cfg : configs) {
            Options opts(options_for_config(*cfg.first), add_default_parameters(cfg.second, *cfg.first));

            check_error(opts.check());

            if (cfg.first->check_args)
                check_error((cfg.first->check_args)(opts));

            if (m_error)
                return ChannelList();
            else
                ret.emplace_back((cfg.first->create)(cfg.first->name.c_str(), cfg.first->initial_cfg, opts));
        }

        return ret;
    }

    bool add(const char* config_string)
    {
        auto channels = parse(config_string);
        m_channels.insert(m_channels.end(), channels.begin(), channels.end());
        return !m_error;
    }

    std::string get_description_for_spec(const char* name) const
    {
        auto it = m_spec.find(name);
        return it != m_spec.end() ? it->second->description : std::string();
    }

    std::string get_documentation_for_spec(const char* name) const
    {
        std::ostringstream out;
        out << name;

        auto it = m_spec.find(name);
        if (it == m_spec.end()) {
            out << ": Not available";
        } else {
            out << "\n " << it->second->description;
            auto optdescrmap = options_for_config(*it->second).get_option_descriptions();

            if (!optdescrmap.empty()) {
                size_t len = 0;
                for (const auto& op : optdescrmap)
                    len = std::max<size_t>(len, op.first.size());

                out << "\n  Options:";
                for (const auto& op : optdescrmap)
                    util::pad_right(out << "\n   ", op.first, len) << op.second;
            }
        }

        return out.str();
    }

    std::vector<std::string> get_docstrings() const
    {
        std::vector<std::string> ret;

        for (const auto& p : m_spec)
            ret.push_back(get_documentation_for_spec(p.first.c_str()));

        return ret;
    }

    ConfigManagerImpl()
        : builtin_option_specs_list({
#ifdef CALIPER_HAVE_GOTCHA
              builtin_gotcha_option_specs,
#endif
#ifdef CALIPER_HAVE_MPI
                  builtin_mpi_option_specs,
#endif
#ifdef CALIPER_HAVE_OMPT
                  builtin_openmp_option_specs,
#endif
#ifdef CALIPER_HAVE_CUPTI
                  builtin_cuda_option_specs,
#endif
#if defined(CALIPER_HAVE_ROCTRACER) || defined(CALIPER_HAVE_ROCPROFILER)
                  builtin_rocm_option_specs,
#endif
#ifdef CALIPER_HAVE_LIBDW
                  builtin_libdw_option_specs,
#endif
#ifdef CALIPER_HAVE_PCP
                  builtin_pcp_option_specs,
#endif
#ifdef CALIPER_HAVE_UMPIRE
                  builtin_umpire_option_specs,
#endif
#ifdef CALIPER_HAVE_KOKKOS
                  builtin_kokkos_option_specs,
#endif
                  builtin_base_option_specs
          })
    {
#ifdef CALIPER_HAVE_PAPI
#ifdef CALIPER_HAVE_ARCH
        std::string cali_arch = CALIPER_HAVE_ARCH;
        Log(2).stream() << "ConfigManager: detected architecture " << cali_arch << std::endl;
        if (cali_arch == "sapphirerapids") {
            builtin_option_specs_list.push_back(builtin_papi_spr_option_specs);
        } else if (cali_arch == "skylake" || cali_arch == "skylake_avx512" || cali_arch == "cascadelake") {
            builtin_option_specs_list.push_back(builtin_papi_skl_option_specs);
        } else {
            builtin_option_specs_list.push_back(builtin_papi_hsw_option_specs);
        }
#else
        builtin_option_specs_list.push_back(builtin_papi_hsw_option_specs);
#endif
#endif
        for (const char* spec_p : builtin_option_specs_list)
            add_global_option_specs(spec_p);
    }
};

ConfigManager::ConfigManager() : mP(new ConfigManagerImpl)
{}

ConfigManager::ConfigManager(const char* config_string) : mP(new ConfigManagerImpl)
{
    mP->add(config_string);
}

ConfigManager::~ConfigManager()
{
    mP.reset();
}

void ConfigManager::add_config_spec(const ConfigInfo& info)
{
    mP->add_config_spec(info.spec, info.create, info.check_args, false /* treat existing spec as error */);
}

void ConfigManager::add_config_spec(const char* json)
{
    ConfigInfo info { json, nullptr, nullptr };
    add_config_spec(info);
}

void ConfigManager::add_option_spec(const char* json)
{
    mP->add_global_option_specs(json);
}

ConfigManager::ChannelList ConfigManager::parse(const char* config_str)
{
    return mP->parse(config_str);
}

void ConfigManager::load(const char* filename)
{
    mP->load_file(filename);
}

bool ConfigManager::add(const char* config_str)
{
    mP->add(config_str);

    if (!mP->m_extra_vars.empty())
        mP->set_error("Unknown config or parameter: " + mP->m_extra_vars.begin()->first);

    return !mP->m_error;
}

bool ConfigManager::add(const char* config_string, argmap_t& extra_kv_pairs)
{
    mP->add(config_string);

    extra_kv_pairs.insert(mP->m_extra_vars.begin(), mP->m_extra_vars.end());

    return !mP->m_error;
}

bool ConfigManager::error() const
{
    return mP->m_error;
}

std::string ConfigManager::error_msg() const
{
    return mP->m_error_msg;
}

void ConfigManager::set_default_parameter(const char* key, const char* value)
{
    mP->m_default_parameters.push_back(std::make_pair(key, value));
}

void ConfigManager::set_default_parameter_for_config(const char* config, const char* key, const char* value)
{
    mP->m_default_parameters_for_spec[config].push_back(std::make_pair(key, value));
}

ConfigManager::ChannelList ConfigManager::get_all_channels() const
{
    return mP->m_channels;
}

ConfigManager::ChannelPtr ConfigManager::get_channel(const char* name) const
{
    for (ChannelPtr& chn : mP->m_channels)
        if (chn->name() == name)
            return chn;

    return ChannelPtr();
}

void ConfigManager::start()
{
    for (ChannelPtr& chn : mP->m_channels)
        chn->start();
}

void ConfigManager::stop()
{
    for (ChannelPtr& chn : mP->m_channels)
        chn->stop();
}

void ConfigManager::flush()
{
    for (ChannelPtr& chn : mP->m_channels)
        chn->flush();
}

std::string ConfigManager::check(const char* configstr, bool allow_extra_kv_pairs) const
{
    // Make a copy of our data because parsing the string modifies its state
    ConfigManagerImpl tmpP(*mP);
    auto              configs = tmpP.parse_configstring(configstr);

    for (auto cfg : configs) {
        Options opts(tmpP.options_for_config(*cfg.first), tmpP.add_default_parameters(cfg.second, *cfg.first));

        if (cfg.first->check_args)
            tmpP.check_error((cfg.first->check_args)(opts));

        tmpP.check_error(opts.check());

        if (tmpP.m_error)
            break;
    }

    if (!allow_extra_kv_pairs && !tmpP.m_extra_vars.empty())
        tmpP.set_error("Unknown config or parameter: " + tmpP.m_extra_vars.begin()->first);

    return tmpP.m_error_msg;
}

std::vector<std::string> ConfigManager::available_config_specs() const
{
    mP->import_builtin_config_specs();

    std::vector<std::string> ret;
    for (const auto& p : mP->m_spec)
        ret.push_back(p.first);

    return ret;
}

std::string ConfigManager::get_description_for_spec(const char* name) const
{
    mP->import_builtin_config_specs();
    return mP->get_description_for_spec(name);
}

std::string ConfigManager::get_documentation_for_spec(const char* name) const
{
    mP->import_builtin_config_specs();
    return mP->get_documentation_for_spec(name);
}

std::vector<std::string> ConfigManager::available_configs()
{
    return ConfigManager().available_config_specs();
}

std::vector<std::string> ConfigManager::get_config_docstrings()
{
    ConfigManagerImpl mgr;
    mgr.import_builtin_config_specs();
    return mgr.get_docstrings();
}

std::string ConfigManager::check_config_string(const char* configstr, bool allow_extra_kv_pairs)
{
    return ConfigManager().check(configstr, allow_extra_kv_pairs);
}

void cali::add_global_config_specs(const ConfigManager::ConfigInfo** configs)
{
    ::ConfigSpecManager::instance()->add_controller_specs(configs);
}
