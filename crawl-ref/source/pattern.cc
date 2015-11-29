#include "AppHdr.h"

#ifdef REGEX_PCRE
    // Statically link pcre on Windows
    #if defined(TARGET_OS_WINDOWS)
        #define PCRE_STATIC
    #endif

    #include <pcre.h>
#endif

#ifdef REGEX_POSIX
    #include <regex.h>
#endif

#include "pattern.h"
#include "stringutil.h"

#if defined(REGEX_PCRE)
////////////////////////////////////////////////////////////////////
// Perl Compatible Regular Expressions

static void *_compile_pattern(const char *pattern, bool icase)
{
    const char *error;
    int erroffset;
    int flags = icase ? PCRE_CASELESS : 0;
    return pcre_compile(pattern,
                        flags,
                        &error,
                        &erroffset,
                        nullptr);
}

static void _free_compiled_pattern(void *cp)
{
    if (cp)
        pcre_free(cp);
}

static bool _pattern_match(void *compiled_pattern, const char *text, int length)
{
    int ovector[42];
    int pcre_rc = pcre_exec(static_cast<pcre *>(compiled_pattern),
                            nullptr,
                            text, length, 0, 0,
                            ovector, sizeof(ovector) / sizeof(*ovector));
    return pcre_rc >= 0;
}

static pattern_match _pattern_match_location(void *compiled_pattern,
                                             const char *text, int length)
{
    int ovector[42];
    int pcre_rc = pcre_exec(static_cast<pcre *>(compiled_pattern),
                            nullptr,
                            text, length, 0, 0,
                            ovector, sizeof(ovector) / sizeof(*ovector));
    if (pcre_rc >= 0)
        return pattern_match::succeeded(string(text), ovector[0], ovector[1]);
    else
        return pattern_match::failed(string(text));
}

////////////////////////////////////////////////////////////////////
#else
////////////////////////////////////////////////////////////////////
// POSIX regular expressions

static void *_compile_pattern(const char *pattern, bool icase)
{
    regex_t *re = new regex_t;
    if (!re)
        return nullptr;

    int flags = REG_EXTENDED;
    if (icase)
        flags |= REG_ICASE;
    int rc = regcomp(re, pattern, flags);
    // Nonzero return code == failure
    if (rc)
    {
        delete re;
        return nullptr;
    }
    return re;
}

static void _free_compiled_pattern(void *cp)
{
    if (cp)
    {
        regex_t *re = static_cast<regex_t *>(cp);
        regfree(re);
        delete re;
    }
}

static bool _pattern_match(void *compiled_pattern, const char *text, int length)
{
    regex_t *re = static_cast<regex_t *>(compiled_pattern);
    return !regexec(re, text, 0, nullptr, 0);
}

static pattern_match _pattern_match_location(void *compiled_pattern,
                                             const char *text, int length)
{
    regmatch_t match;
    regex_t *re = static_cast<regex_t *>(compiled_pattern);
    if (!regexec(re, text, 1, &match, 0))
        return pattern_match::succeeded(string(text), match.rm_so, match.rm_eo);
    else
        return pattern_match::failed(string(text));
}

////////////////////////////////////////////////////////////////////
#endif

string pattern_match::annotate_string(const string &color) const
{
    string ret(text);

    if (*this && start < end)
    {
        ret.insert(end, make_stringf("</%s>", color.c_str()));
        ret.insert(start, make_stringf("<%s>", color.c_str()));
    }

    size_t pos = string::npos;
    while ((pos = ret.find('\n')) != string::npos)
        ret.replace(pos, 1, " ");

    return ret;
}

text_pattern::~text_pattern()
{
    if (compiled_pattern)
        _free_compiled_pattern(compiled_pattern);
}

const text_pattern &text_pattern::operator= (const text_pattern &tp)
{
    if (this == &tp)
        return tp;

    if (compiled_pattern)
        _free_compiled_pattern(compiled_pattern);
    pattern = tp.pattern;
    compiled_pattern = nullptr;
    isvalid      = tp.isvalid;
    ignore_case  = tp.ignore_case;
    return *this;
}

const text_pattern &text_pattern::operator= (const string &spattern)
{
    if (pattern == spattern)
        return *this;

    if (compiled_pattern)
        _free_compiled_pattern(compiled_pattern);
    pattern = spattern;
    compiled_pattern = nullptr;
    isvalid = true;
    // We don't change ignore_case
    return *this;
}

bool text_pattern::operator== (const text_pattern &tp) const
{
    if (this == &tp)
        return true;

    return pattern == tp.pattern && ignore_case == tp.ignore_case;
}

bool text_pattern::compile() const
{
    return !empty()?
        !!(compiled_pattern = _compile_pattern(pattern.c_str(), ignore_case))
      : false;
}

bool text_pattern::matches(const char *s, int length) const
{
    return valid() && _pattern_match(compiled_pattern, s, length);
}

pattern_match text_pattern::match_location(const char *s, int length) const
{
    if (valid())
        return _pattern_match_location(compiled_pattern, s, length);
    else
        return pattern_match::failed(string(s));
}
