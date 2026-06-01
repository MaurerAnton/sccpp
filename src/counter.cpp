     1|#include "counter.hpp"
     2|#include "language.hpp"
     3|#include <cstring>
     4|#include <algorithm>
     5|#include <cstdio>
     6|
     7|/* Forward declarations */
     8|static bool isWhitespace(uint8_t b);
     9|static bool isBinary(int index, uint8_t curByte);
    10|static bool checkForMatchSingle(const uint8_t* content, int index, int endPoint, const std::vector<uint8_t>& matches);
    11|static void countComplexityPostfix(FileJob* job, int index, int offsetJump, const std::vector<std::vector<uint8_t>>& excludes);
    12|static bool hasNonWhitespaceBefore(const std::vector<uint8_t>& content, int idx);
    13|static int nextNonWhitespaceIndex(const std::vector<uint8_t>& content, int idx);
    14|static bool isIdentContinue(uint8_t b);
    15|static bool hasPostfixExclude(const std::vector<uint8_t>& content, int index, int offsetJump, const std::vector<std::vector<uint8_t>>& excludes);
    16|
    17|void countStats(FileJob* job) {
    18|    if (job->bytes == 0) {
    19|        job->lines = 0;
    20|        return;
    21|    }
    22|
    23|    /* Map is read-only after preload, no lock needed */
    24|    auto it = languageFeatures.find(job->language);
    25|    if (it == languageFeatures.end()) return;
    26|    LanguageFeature& feat = it->second;
    27|
    28|    int endPoint = (int)job->bytes - 1;
    29|    int64_t currentState = S_BLANK;
    30|    std::vector<std::vector<uint8_t>> endComments;
    31|    std::vector<uint8_t> endString;
    32|    bool ignoreEscape = false;
    33|
    34|    if (job->trackComplexityLines) {
    35|        job->complexityLine.push_back(0);
    36|    }
    37|
    38|    if (job->classifyContent) {
    39|        job->contentByteType.assign(job->bytes, 0);
    40|    }
    41|
    42|    /* Skip BOM if present */
    43|    auto bomSkip = [](const std::vector<uint8_t>& c) -> int {
    44|        if (c.size() >= 3 && c[0] == 0xEF && c[1] == 0xBB && c[2] == 0xBF) return 3;
    45|        return 0;
    46|    };
    47|    int startIdx = bomSkip(job->content);
    48|
    49|    auto& content = job->content;
    50|
    51|    for (int index = startIdx; index < (int)content.size(); index++) {
    52|        if (job->classifyContent) {
    53|            switch (currentState) {
    54|                case S_CODE: job->contentByteType[index] = BYTE_CODE; break;
    55|                case S_STRING: job->contentByteType[index] = BYTE_STRING; break;
    56|                case S_COMMENT: case S_COMMENT_CODE: case S_MULTICOMMENT:
    57|                case S_MULTICOMMENT_CODE: case S_MULTICOMMENT_BLANK: case S_DOCSTRING:
    58|                    job->contentByteType[index] = BYTE_COMMENT; break;
    59|                default: job->contentByteType[index] = BYTE_BLANK; break;
    60|            }
    61|        }
    62|
    63|        if (!isWhitespace(content[index])) {
    64|            switch (currentState) {
    65|                case S_CODE: {
    66|                    /* codeState */
    67|                    for (int i = index; i <= endPoint; i++) {
    68|                        index = i;
    69|                        if (job->classifyContent && (size_t)i < job->contentByteType.size())
    70|                            job->contentByteType[i] = BYTE_CODE;
    71|
    72|                        if (content[i] == '\n') goto endProcessLine;
    73|
    74|                        if (isBinary(i, content[i])) {
    75|                            job->binary = true;
    76|                            return;
    77|                        }
    78|
    79|                        if (content[i] & feat.processMask) {
    80|                            auto m = feat.tokens->match(&content[i], content.size() - i);
    81|                            if (m.type != 0) {
    82|                                bool match_ok = true;
    83|                                /* Verify the match fully */
    84|                                for (int k = 0; k < m.depth + 1 && i + k < (int)content.size(); k++) {
    85|                                    /* Already matched by Trie */
    86|                                    (void)k;
    87|                                }
    88|                                (void)match_ok;
    89|
    90|                                switch (m.type) {
    91|                                    case T_STRING: {
    92|                                        /* check for ignoreEscape */
    93|                                        bool ie = false;
    94|                                        for (auto& q : feat.quotes) {
    95|                                            if ((q.ignoreEscape || q.docString) && !q.start.empty()) {
    96|                                                bool match = true;
    97|                                                for (size_t j = 0; j < q.start.size() && i + j < content.size(); j++) {
    98|                                                    if (content[i + j] != (uint8_t)q.start[j]) { match = false; break; }
    99|                                                }
   100|                                                if (match) { ie = q.ignoreEscape; break; }
   101|                                            }
   102|                                        }
   103|                                        if (i > 0 && content[i - 1] != '\\') {
   104|                                            currentState = S_STRING;
   105|                                        }
   106|                                        endString = m.close;
   107|                                        ignoreEscape = ie;
   108|                                        goto endSwitch;
   109|                                    }
   110|                                    case T_SLCOMMENT:
   111|                                        currentState = S_COMMENT_CODE;
   112|                                        goto endSwitch;
   113|                                    case T_MLCOMMENT:
   114|                                        if (feat.nested || endComments.empty()) {
   115|                                            endComments.push_back(m.close);
   116|                                            currentState = S_MULTICOMMENT_CODE;
   117|                                            i += m.depth - 1;
   118|                                        }
   119|                                        goto endSwitch;
   120|                                    case T_COMPLEXITY:
   121|                                        if (index == 0 || isWhitespace(content[index - 1])) {
   122|                                            job->complexity++;
   123|                                            if (!job->complexityLine.empty()) job->complexityLine.back()++;
   124|                                        }
   125|                                        goto endSwitch;
   126|                                    case T_COMPLEXITY_POSTFIX:
   127|                                        countComplexityPostfix(job, index, m.depth + 1, feat.postfixExcludes);
   128|                                        goto endSwitch;
   129|                                }
   130|                            }
   131|                        }
   132|                    }
   133|                    goto endProcessLine;
   134|                }
   135|
   136|                case S_STRING: {
   137|                    for (int i = index; i <= endPoint; i++) {
   138|                        index = i;
   139|                        if (job->classifyContent && (size_t)i < job->contentByteType.size())
   140|                            job->contentByteType[i] = BYTE_STRING;
   141|
   142|                        if (content[i] == '\n') goto endProcessLine;
   143|
   144|                        /* Check for escape */
   145|                        bool escaped = false;
   146|                        if (i > 0 && content[i - 1] == '\\') {
   147|                            int numEscapes = 0;
   148|                            for (int j = i - 1; j > 0 && content[j] == '\\'; j--) numEscapes++;
   149|                            if (numEscapes % 2 != 0) escaped = true;
   150|                        }
   151|
   152|                        if (ignoreEscape || !escaped) {
   153|                            if (!endString.empty() && checkForMatchSingle(content.data(), i, endPoint, endString)) {
   154|                                currentState = S_CODE;
   155|                                goto endSwitch;
   156|                            }
   157|                        }
   158|                    }
   159|                    goto endProcessLine;
   160|                }
   161|
   162|                case S_DOCSTRING: {
   163|                    for (int i = index; i <= endPoint; i++) {
   164|                        index = i;
   165|                        if (job->classifyContent && (size_t)i < job->contentByteType.size())
   166|                            job->contentByteType[i] = BYTE_COMMENT;
   167|
   168|                        if (content[i] == '\n') goto endProcessLine;
   169|
   170|                        if (i > 0 && content[i - 1] != '\\') {
   171|                            if (!endString.empty() && checkForMatchSingle(content.data(), i, endPoint, endString)) {
   172|                                /* Check after closing for whitespace-only */
   173|                                for (int j = i + (int)endString.size(); j <= endPoint; j++) {
   174|                                    if (content[j] == '\n') {
   175|                                        currentState = S_COMMENT;
   176|                                        goto endSwitch;
   177|                                    }
   178|                                    if (!isWhitespace(content[j])) {
   179|                                        currentState = S_CODE;
   180|                                        goto endSwitch;
   181|                                    }
   182|                                }
   183|                                currentState = S_CODE;
   184|                                goto endSwitch;
   185|                            }
   186|                        }
   187|                    }
   188|                    goto endProcessLine;
   189|                }
   190|
   191|                case S_MULTICOMMENT:
   192|                case S_MULTICOMMENT_CODE: {
   194|                    bool innerBreak = false;
   195|                    for (int i = index; i <= endPoint; i++) {
   196|                        index = i;
   198|                        if (job->classifyContent && (size_t)i < job->contentByteType.size())
   199|                            job->contentByteType[i] = BYTE_COMMENT;
   200|
   201|                        if (content[i] == '\n') { innerBreak = true; goto endProcessLineInner; }
   202|
   203|                        if (!endComments.empty()) {
   204|                            if (checkForMatchSingle(content.data(), i, endPoint, endComments.back())) {
   205|                                int oj = (int)endComments.back().size();
   206|                                endComments.pop_back();
   207|                                if (endComments.empty()) {
   208|                                    currentState = (currentState == S_MULTICOMMENT_CODE) ? S_CODE : S_MULTICOMMENT_BLANK;
   210|                                }
   211|                                i += oj - 1;
   212|                                innerBreak = true;
   213|                                break; /* break inner for loop */
   214|                            }
   215|                        }
   216|
   217|                        /* Check for nested multi-line comment entry */
   218|                        if (feat.nested || endComments.empty()) {
   219|                            auto m = feat.multiLineComments->match(&content[i], content.size() - i);
   220|                            if (m.type == T_MLCOMMENT) {
   221|                                endComments.push_back(m.close);
   222|                                i += m.depth - 1;
   223|                                innerBreak = true;
   224|                                break;
   225|                            }
   226|                        }
   227|                    }
   228|                    /* Fall through to endProcessLine or continue main loop */
   229|                    if (index >= (int)content.size()) return;
   230|                    endProcessLineInner:
   231|                    if (!innerBreak) goto endProcessLine; /* newline from inner loop */
   232|                    /* else: inner break (close found), continue main loop */
   233|                    break;
   234|                }
   235|
   236|                case S_BLANK:
   237|                case S_MULTICOMMENT_BLANK: {
   238|                    auto m = feat.tokens->match(&content[index], content.size() - index);
   239|                    switch (m.type) {
   240|                        case T_MLCOMMENT:
   241|                            if (feat.nested || endComments.empty()) {
   242|                                endComments.push_back(m.close);
   243|                                currentState = S_MULTICOMMENT;
   244|                                index += m.depth - 1;
   245|                                if (job->classifyContent && (size_t)index < job->contentByteType.size())
   246|                                    job->contentByteType[index] = BYTE_COMMENT;
   247|                            }
   248|                            break;
   249|                        case T_SLCOMMENT:
   250|                            currentState = S_COMMENT;
   251|                            if (job->classifyContent && (size_t)index < job->contentByteType.size())
   252|                                job->contentByteType[index] = BYTE_COMMENT;
   253|                            break;
   254|                        case T_STRING: {
   255|                            bool ie = false;
   256|                            for (auto& q : feat.quotes) {
   257|                                if ((q.ignoreEscape || q.docString) && !q.start.empty()) {
   258|                                    bool match = true;
   259|                                    for (size_t j = 0; j < q.start.size() && index + j < content.size(); j++) {
   260|                                        if (content[index + j] != (uint8_t)q.start[j]) { match = false; break; }
   261|                                    }
   262|                                    if (match) { ie = q.ignoreEscape; break; }
   263|                                }
   264|                            }
   265|                            ignoreEscape = ie;
   266|                            endString = m.close;
   267|
   268|                            /* Check if docstring */
   269|                            bool isDS = false;
   270|                            for (auto& q : feat.quotes) {
   271|                                if (q.docString && q.end == std::string(endString.begin(), endString.end())) {
   272|                                    isDS = true;
   273|                                    break;
   274|                                }
   275|                            }
   276|                            currentState = isDS ? S_DOCSTRING : S_STRING;
   277|                            if (job->classifyContent && (size_t)index < job->contentByteType.size()) {
   278|                                job->contentByteType[index] = isDS ? BYTE_COMMENT : BYTE_STRING;
   279|                            }
   280|                            break;
   281|                        }
   282|                        case T_COMPLEXITY:
   283|                            currentState = S_CODE;
   284|                            if (job->classifyContent && (size_t)index < job->contentByteType.size())
   285|                                job->contentByteType[index] = BYTE_CODE;
   286|                            if (index == 0 || isWhitespace(content[index - 1])) {
   287|                                job->complexity++;
   288|                                if (!job->complexityLine.empty()) job->complexityLine.back()++;
   289|                            }
   290|                            break;
   291|                        case T_COMPLEXITY_POSTFIX:
   292|                            currentState = S_CODE;
   293|                            if (job->classifyContent && (size_t)index < job->contentByteType.size())
   294|                                job->contentByteType[index] = BYTE_CODE;
   295|                            countComplexityPostfix(job, index, m.depth + 1, feat.postfixExcludes);
   296|                            break;
   297|                        default:
   298|                            currentState = S_CODE;
   299|                            if (job->classifyContent && (size_t)index < job->contentByteType.size())
   300|                                job->contentByteType[index] = BYTE_CODE;
   301|                            break;
   302|                    }
   303|                    break;
   304|                }
   305|            }
   306|            endSwitch:;
   307|        }
   308|
   309|        if (index >= (int)content.size()) return;
   310|
   311|        if (index < 10000 && job->binary) return;
   312|
   313|        endProcessLine:
   314|        if (content[index] == '\n' || index >= endPoint) {
   315|            job->lines++;
   316|            if (job->trackComplexityLines) {
   317|                job->complexityLine.push_back(0);
   318|            }
   319|
   320|            switch (currentState) {
   321|                case S_CODE: case S_STRING: case S_COMMENT_CODE: case S_MULTICOMMENT_CODE:
   322|                    job->code++;
   323|                    currentState = (currentState == S_CODE || currentState == S_COMMENT_CODE ||
   324|                                    currentState == S_MULTICOMMENT_CODE) ? S_BLANK : currentState;
   325|                    if (currentState == S_STRING) currentState = S_STRING;
   326|                    if (currentState == S_MULTICOMMENT) currentState = S_MULTICOMMENT;
   327|                    /* resetState logic */
   328|                    if (currentState != S_MULTICOMMENT && currentState != S_MULTICOMMENT_CODE && currentState != S_STRING)
   329|                        currentState = S_BLANK;
   330|                    break;
   331|                case S_COMMENT: case S_MULTICOMMENT: case S_MULTICOMMENT_BLANK:
   332|                    job->comment++;
   333|                    if (currentState == S_MULTICOMMENT || currentState == S_MULTICOMMENT_CODE)
   334|                        currentState = S_MULTICOMMENT;
   335|                    else if (currentState == S_STRING)
   336|                        currentState = S_STRING;
   337|                    else
   338|                        currentState = S_BLANK;
   339|                    break;
   340|                case S_BLANK:
   341|                    job->blank++;
   342|                    break;
   343|                case S_DOCSTRING:
   344|                    job->comment++;
   345|                    currentState = S_BLANK;
   346|                    break;
   347|                default:
   348|                    job->blank++;
   349|                    break;
   350|            }
   351|        }
   352|    }
   353|
   354|    /* Note: feat tries are owned by languageFeatures, not cleaned up here */
   355|}
   356|
   357|static bool isWhitespace(uint8_t b) {
   358|    return b == ' ' || b == '\t' || b == '\n' || b == '\r';
   359|}
   360|
   361|static bool isBinary(int index, uint8_t curByte) {
   362|    return index < 10000 && curByte == 0;
   363|}
   364|
   365|static bool checkForMatchSingle(const uint8_t* content, int index, int endPoint, const std::vector<uint8_t>& matches) {
   366|    if (matches.empty()) return false;
   367|    if (content[index] != matches[0]) return false;
   368|    for (size_t j = 0; j < matches.size(); j++) {
   369|        if (index + (int)j >= endPoint + 1 || matches[j] != content[index + j]) return false;
   370|    }
   371|    return true;
   372|}
   373|
   374|static bool hasNonWhitespaceBefore(const std::vector<uint8_t>& content, int idx) {
   375|    for (int i = idx - 1; i >= 0; i--) {
   376|        if (!isWhitespace(content[i])) return true;
   377|    }
   378|    return false;
   379|}
   380|
   381|static int nextNonWhitespaceIndex(const std::vector<uint8_t>& content, int idx) {
   382|    while (idx < (int)content.size() && isWhitespace(content[idx])) idx++;
   383|    return idx;
   384|}
   385|
   386|static bool isIdentContinue(uint8_t b) {
   387|    return (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || (b >= '0' && b <= '9') || b == '_';
   388|}
   389|
   390|static bool hasPostfixExclude(const std::vector<uint8_t>& content, int index, int offsetJump, const std::vector<std::vector<uint8_t>>& excludes) {
   391|    if (index + offsetJump > (int)content.size()) return false;
   392|
   393|    for (auto& exclude : excludes) {
   394|        if ((int)exclude.size() < offsetJump) continue;
   395|
   396|        bool match = true;
   397|        for (int k = 0; k < offsetJump; k++) {
   398|            if (content[index + k] != exclude[k]) { match = false; break; }
   399|        }
   400|        if (!match) continue;
   401|
   402|        if ((int)exclude.size() == offsetJump) return true;
   403|
   404|        int next = nextNonWhitespaceIndex(content, index + offsetJump);
   405|        int remaining = (int)exclude.size() - offsetJump;
   406|        if (next + remaining > (int)content.size()) continue;
   407|
   408|        match = true;
   409|        for (int k = 0; k < remaining; k++) {
   410|            if (content[next + k] != exclude[offsetJump + k]) { match = false; break; }
   411|        }
   412|        if (!match) continue;
   413|
   414|        int afterExclude = next + remaining;
   415|        if (isIdentContinue(exclude.back())) {
   416|            return afterExclude == (int)content.size() || !isIdentContinue(content[afterExclude]);
   417|        }
   418|        return true;
   419|    }
   420|    return false;
   421|}
   422|
   423|static void countComplexityPostfix(FileJob* job, int index, int offsetJump, const std::vector<std::vector<uint8_t>>& excludes) {
   424|    if (index == 0) return;
   425|
   426|    auto& content = job->content;
   427|    if (isWhitespace(content[index - 1]) && !hasNonWhitespaceBefore(content, index - 1)) return;
   428|
   429|    if (!excludes.empty() && hasPostfixExclude(content, index, offsetJump, excludes)) return;
   430|
   431|    job->complexity++;
   432|    if (!job->complexityLine.empty()) job->complexityLine.back()++;
   433|}
   434|