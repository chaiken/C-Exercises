/*
  https://en.cppreference.com/c/string/multibyte
  https://en.cppreference.com/c/string/multibyte/mbstowcs
  https://en.cppreference.com/c/string/multibyte/wcstombs
  https://en.cppreference.com/c/string/multibyte/wcsrtombs
 */
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

void test_wchar(wchar_t wc, locale_t loc) {
  int nchar = wctob(wc);
  if (EOF == nchar) {
    printf("%c could not be narrowed by wctob\n", (char)nchar);
  }
  wprintf(L"wchar_t %lc is %ls alphanumeric\n", wc, iswalpha_l(wc, loc) ? L"indeed" : L"not");
  printf("(char)wchar_t %c is %s alphanumeric\n", (char)wc, isalpha((char)wc) ? "indeed" : "not");
  printf("wctob(wchar_t) %c is %s alphanumeric\n\n", wctob(wc), isalpha(wctob(wc)) ? "indeed" : "not");
}

int main(void)
{
    // 4 wide characters
    const wchar_t src[] = L"z\u00df\u6c34\U0001f34c";
    // they occupy 10 bytes in UTF-8
    char dst[11];

    setlocale(LC_ALL, "en_US.utf8");
    printf("wide-character string: '%ls'\n",src);
    for (size_t ndx = 0; ndx < sizeof src / sizeof src[0]; ++ndx)
        printf("   src[%2zu] = %#8x\n", ndx, src[ndx]);

    int rtn_val = wcstombs(dst, src, sizeof dst);
    printf("rtn_val = %d\n", rtn_val);
    if (rtn_val > 0)
        printf("multibyte string:  '%s'\n",dst);
    for (size_t ndx = 0; ndx < sizeof dst; ++ndx)
        printf("   dst[%2zu] = %#2x\n", ndx, (unsigned char)dst[ndx]);

    printf("\n*******************************\n");
    printf("\nen_US.utf8\n");

    locale_t loc = newlocale(LC_ALL_MASK, "en_US.utf8", (locale_t)0);

    test_wchar(L'ä', loc);
    test_wchar(u'ä', loc);
    test_wchar('a', loc);

    printf("\nASCII chars as wchar_t:\n");
    wchar_t notwide[] = L"abc";
    printf("not-wide-character string: '%ls'\n",notwide);
    for (size_t ndx = 0; ndx < sizeof notwide / sizeof notwide[0]; ++ndx)
        printf("   src[%2zu] = %#8x\n", ndx, notwide[ndx]);
    printf("\n");


    const wchar_t winput[] = L"äaöoüußs";
    const char input[] = "äaöouüßs";

    printf("winput string:\n");
    for (size_t ndx = 0; ndx < sizeof winput / sizeof winput[0]; ++ndx)
        printf("   src[%2zu] = %#8x\n", ndx, winput[ndx]);
    size_t wlen = wcslen(winput);
    size_t slen = strlen(input);
    printf("strlen(input): %lu, wcslen(input): %lu\n", slen, wlen);
    printf("sizeof(ä): %lu\n", sizeof(L"ä"));
    printf("sizeof(a): %lu\n", sizeof("a"));
    char *converted = (char*)malloc(slen + 1);
    int convnum = wcstombs(converted, winput, slen);
    converted[slen] = '\0';
    printf("converted %d, result: %s\n", convnum, converted);
    printf("strlen(converted): %lu\n", strlen(converted));
    printf("converted string:\n");
    for (size_t ndx = 0; ndx < slen; ++ndx)
        printf("   src[%2zu] = %#8x\n", ndx, converted[ndx]);
    free(converted);

    // Has no L'\0', so causes stack overflow.
    // printf("wcslen of %s is %lu and strlen is %lu\n", input, wcslen((wchar_t*)input), strlen(input));
    char nullterm[18];
    strncpy(nullterm, "äaöouüßs", 13);
    wcsncat((wchar_t *)nullterm, L"\0", 4);
    printf("\nnarrowing of wchar_t winput: strlen of %s is %lu\n", (char *)winput, strlen((char *)winput));
    wprintf(L"widening of char input: wcslen of %ls is %lu and strlen is %lu\n", (wchar_t *)nullterm, wcslen((wchar_t*)nullterm), strlen(nullterm));                                  /* No output */

    wprintf(L"with wprintf: L0-terminated char* is %ls\n", (wchar_t *)nullterm);  /* No output */
    printf("with printf: nullterm is %s\n", nullterm);
    // error: multi-character character constant [-Werror=multichar]
    // printf("ä is %s alphabetic\n", (isalpha((char)'ä') ? "indeed" : "not"));
    wprintf(L"wide ä is %ls alphabetic\n", (iswalpha(L'ä') ? L"indeed" : L"not"));


    printf("\n*******************************\n");
    printf("\nde_DE.utf8\n");

   loc = newlocale(LC_ALL_MASK, "de_DE.utf8", (locale_t)0);

    test_wchar(L'ä', loc);
    test_wchar(u'ä', loc);
    test_wchar('a', loc);

    setlocale(LC_ALL, "de_DE.utf8");
    printf("winput string:\n");
    for (size_t ndx = 0; ndx < sizeof winput / sizeof winput[0]; ++ndx)
        printf("   src[%2zu] = %#2x\n", ndx, winput[ndx] & 0xff);
    wlen = wcslen(winput);
    slen = strlen(input);
    printf("strlen(input): %lu, wcslen(input): %lu\n", slen, wlen);
    printf("sizeof(ä): %lu\n", sizeof(L'ä'));
    printf("sizeof(a): %lu\n", sizeof("a"));
    converted = (char*)malloc(slen + 1);
    convnum = wcstombs(converted, winput, slen);
    converted[slen] = '\0';
    printf("converted %d, result: %s\n", convnum, converted);
    printf("strlen(converted): %lu\n", strlen(converted));
    printf("converted string:\n");
    for (size_t ndx = 0; ndx < slen; ++ndx)
        printf("   src[%2zu] = %#2x\n", ndx, converted[ndx] & 0xff);
    free(converted);

    printf("\nnarrowing of wchar_t winput: strlen of %s is %lu\n", (char *)winput, strlen((char *)winput));
    //    printf("wcslen of %s is %lu and strlen is %lu\n", input, wcslen((wchar_t *)input), strlen(input));
    wprintf(L"widening of char input: wcslen of %ls is %lu and strlen is %lu\n", (wchar_t *)nullterm, wcslen((wchar_t*)nullterm), strlen(nullterm));                                  /* No output */

    wprintf(L"L0-terminated char* is %ls\n", (wchar_t *)nullterm);
    printf("with printf: nullterm is %s\n", nullterm);
    // printf("ä is %s alphabetic\n", (isalpha((char)'ä') ? "indeed" : "not"));
    wprintf(L"with wprintf: L0-terminated char* is %ls\n", (wchar_t *)nullterm);  /* No output */

    printf("\n*******************************\n");
    printf("\nde_DE.iso-8859-1\n");
    setlocale(LC_ALL, "de_DE.iso-8859-1");
    printf("winput string:\n");
    for (size_t ndx = 0; ndx < sizeof winput / sizeof winput[0]; ++ndx)
        printf("   src[%2zu] = %#8x\n", ndx, winput[ndx]);

    wlen = wcslen(winput);
    slen = strlen(input);
    printf("strlen(input): %lu, wcslen(input): %lu\n", slen, wlen);
    printf("sizeof(ä): %lu\n", sizeof(L"ä"));
    printf("sizeof(a): %lu\n", sizeof("a"));
    converted = (char*)malloc(slen + 1);
    convnum = wcstombs(converted, winput, slen);
    converted[slen] = '\0';
    printf("converted %d, result: %s\n", convnum, converted);
    printf("strlen(converted): %lu\n", strlen(converted));
    printf("converted string:\n");
    for (size_t ndx = 0; ndx < slen; ++ndx)
        printf("   src[%2zu] = %#8x\n", ndx, converted[ndx]);
    free(converted);


    printf("\nnarrowing of wchar_t winput: strlen of %s is %lu\n", (char *)winput, strlen((char *)winput));
    wprintf(L"widening of char input: wcslen of %ls is %lu and strlen is %lu\n", (wchar_t *)nullterm, wcslen((wchar_t*)nullterm), strlen(nullterm));                                  /* No output */
    wprintf(L"with wprintf: L0-terminated char* is %ls\n", (wchar_t *)nullterm);
    printf("with printf: nullterm is %s\n", nullterm);
    // printf("ä is %s alphabetic\n", (isalpha((char)'ä') ? "indeed" : "not"));
    wprintf(L"wide ä is %ls alphabetic\n", (iswalpha(L'ä') ? L"indeed" : L"not"));
}
