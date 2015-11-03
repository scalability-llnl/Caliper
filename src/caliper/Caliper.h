/** 
 * @file caliper.h 
 * Initialization function and global data declaration
 */

#ifndef CALI_CALIPER_H
#define CALI_CALIPER_H

#include "cali_definitions.h"

#include <Attribute.h>
#include <Record.h>
#include <Variant.h>
#include <util/callback.hpp>

#include <functional>
#include <memory>
#include <utility>


namespace cali
{

// Forward declarations

class ContextBuffer;
class Node;    

template<int> class FixedSnapshot;
typedef FixedSnapshot<64> Snapshot;

/// @class Caliper

class Caliper 
{
    struct CaliperImpl;

    std::unique_ptr<CaliperImpl> mP;


    Caliper();

public:

    ~Caliper();

    Caliper(const Caliper&) = delete;

    Caliper& operator = (const Caliper&) = delete;

    // --- Class for either a node or immediate data element

    class Entry {
        Node*     m_node;
	cali_id_t m_attr_id;
        Variant   m_value;

        Entry() : m_node { nullptr }, m_attr_id { CALI_INV_ID }
            { }

    public:

        /// @brief Return top-level attribute of this entry
        cali_id_t attribute() const;

        /// @brief Count instances of attribute @param attr_id in this entry
        int       count(cali_id_t attr_id = CALI_INV_ID) const;
        int       count(const Attribute& attr) const {
            return count(attr.id());
        }

        /// @brief Return top-level data value of this entry
        Variant   value() const;

        /// @brief Extract data value for attribute @param attr_id from this entry
        Variant   value(cali_id_t attr_id) const;
        Variant   value(const Attribute& attr) const {
            return value(attr.id());
        }

        // int       extract(cali_id_t attr, int n, Variant buf[]) const;

        static const Entry empty;

        friend class CaliperImpl;
    };

    Entry     make_entry(size_t n, const Attribute* attr, const Variant* value);
    Entry     make_entry(const Attribute& attr, const Variant& value);


    // --- Events

    struct Events {
        util::callback<void(Caliper*, const Attribute&)> create_attr_evt;

        util::callback<void(Caliper*, const Attribute&)> pre_begin_evt;
        util::callback<void(Caliper*, const Attribute&)> post_begin_evt;
        util::callback<void(Caliper*, const Attribute&)> pre_end_evt;
        util::callback<void(Caliper*, const Attribute&)> post_end_evt;
        util::callback<void(Caliper*, const Attribute&)> pre_set_evt;
        util::callback<void(Caliper*, const Attribute&)> post_set_evt;

        util::callback<void(cali_context_scope_t, 
                            ContextBuffer*)>             create_context_evt;
        util::callback<void(ContextBuffer*)>             destroy_context_evt;

        util::callback<void(Caliper*)>                   post_init_evt;
        util::callback<void(Caliper*)>                   finish_evt;

        util::callback<void(Caliper*, 
                            int, 
                            const Entry*,
                            Snapshot*)>                  snapshot;
        util::callback<void(Caliper*,
                            const Entry*,
                            const Snapshot*)>            process_snapshot;

        util::callback<void(const RecordDescriptor&,
                            const int*,
                            const Variant**)>            write_record;
    };

    Events&   events();

    // --- Context environment API

    ContextBuffer* default_contextbuffer(cali_context_scope_t context) const;
    ContextBuffer* current_contextbuffer(cali_context_scope_t context);

    ContextBuffer* create_contextbuffer(cali_context_scope_t context);
    void           release_contextbuffer(ContextBuffer*);

    void           set_contextbuffer_callback(cali_context_scope_t context, std::function<ContextBuffer*()> cb);    

    // --- Snapshot API

    void      push_snapshot(int scopes, const Entry* trigger_info);
    void      pull_snapshot(int scopes, const Entry* trigger_info, Snapshot* snapshot);

    // --- Annotation API

    cali_err  begin(const Attribute& attr, const Variant& data);
    cali_err  end(const Attribute& attr);
    cali_err  set(const Attribute& attr, const Variant& data);
    cali_err  set_path(const Attribute& attr, size_t n, const Variant data[]);

    Variant   exchange(const Attribute& attr, const Variant& data);

    // --- Query API

    Entry     get(const Attribute& attr) const;

    // --- Attribute API

    size_t    num_attributes() const;

    Attribute get_attribute(cali_id_t id) const;
    Attribute get_attribute(const std::string& name) const;

    Attribute create_attribute(const std::string& name, cali_attr_type type, int prop = CALI_ATTR_DEFAULT);

    // --- Serialization / data access API

    void      foreach_node(std::function<void(const Node&)>);
    void      foreach_attribute(std::function<void(const Attribute&)>);


    // --- Caliper singleton API

    static Caliper* instance();
    static Caliper* try_instance();
};

} // namespace cali

#endif // CALI_CALIPER_H
