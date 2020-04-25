program fortran_example
    use caliper_mod

    implicit none

    type(ScopeAnnotation) :: f_ann, i_ann
    type(ConfigManager)   :: mgr

    integer               :: i, count, argc

    logical               :: ret
    character(len=:), allocatable :: errmsg
    character(len=256)    :: arg

    call cali_init()

    mgr = ConfigManager_new()

    argc = command_argument_count()
    if (argc .ge. 1) then
        call get_command_argument(1, arg)
        call mgr%add(arg)
        ret = mgr%error()
        if (ret) then
            errmsg = mgr%error_msg()
            write(*,*) 'ConfigManager: ', errmsg
        endif
    else
        call mgr%add('runtime-report')
    endif

    call mgr%start

    f_ann = ScopeAnnotation_begin('main')

    ! Mark "initialization" phase
    i_ann = ScopeAnnotation_begin('init')
    count = 4
    call ScopeAnnotation_end(i_ann)

    call ScopeAnnotation_end(f_ann)

    call mgr%flush
    call ConfigManager_delete(mgr)
end program fortran_example
