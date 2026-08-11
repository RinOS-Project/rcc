int invalid_control_flow(void)
{
    break;
    continue;
    goto missing;
duplicate:
    ;
duplicate:
    ;
    return 0;
}
