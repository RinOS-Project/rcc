int main(void)
{
    const char unicode[] = "\u03a9";
    const char embedded_nul[] = "\0R";
    return sizeof(unicode) == 3 &&
                   (unsigned char)unicode[0] == 0xce &&
                   (unsigned char)unicode[1] == 0xa9 && unicode[2] == 0 &&
                   sizeof(embedded_nul) == 3 && embedded_nul[0] == 0 &&
                   embedded_nul[1] == 'R' && embedded_nul[2] == 0
               ? 0
               : 1;
}
