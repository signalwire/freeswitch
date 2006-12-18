#!./js

/* ============================================================== */

// jslint.js
// 2006-06-12
/*
Copyright (c) 2002 Douglas Crockford  (www.JSLint.com)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

The Software shall be used for Good, not Evil.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


/*
    jslint is a function. It takes two parameters.

        var myResult = jslint(source, option);

    The first parameter is either a string or an array of strings. If it is a
    string, it will be split on '\n' or '\r'. If it is an array of strings, it
    is assumed that each string represents one line. The source can be a
    JavaScript text, or HTML text, or a Konfabulator text.

    The second parameter is an optional object of options which control the
    operation of jslint. All of the options are booleans. All are optional and
    have a default value of false.

    {
        browser    : true if the standard browser globals should be predefined
        cap        : true if upper case HTML should be allowed
        debug      : true if debugger statements should be allowed
        evil       : true if eval should be allowed
        jscript    : true if jscript deviations should be allowed
        laxLineEnd : true if line breaks should not be checked
        passfail   : true if the scan should stop on first error
        plusplus   : true if post increment should not be allowed
        undef      : true if undefined variables are errors
    }

    If it checks out, jslint returns true. Otherwise, it returns false.

    If false, you can inspect jslint.errors to find out the problems.
    jslint.errors is an array of objects containing these members:

    {
        line      : The line (relative to 0) at which the lint was found
        character : The character (relative to 0) at which the lint was found
        reason    : The problem
        evidence  : The text line in which the problem occurred
    }

    If a fatal error was found, a null will be the last element of the
    jslint.errors array.

    You can request a Function Report, which shows all of the functions
    and the parameters and vars that they use. This can be used to find
    implied global variables and other problems. The report is in HTML and
    can be inserted in a <body>.

        var myReport = jslint.report(option);

    If the option is true, then the report will be limited to only errors.
*/

String.prototype.entityify = function () {
    return this.
        replace(/&/g, '&amp;').
        replace(/</g, '&lt;').
        replace(/>/g, '&gt;');
};

String.prototype.isAlpha = function () {
    return (this >= 'a' && this <= 'z\uffff') ||
        (this >= 'A' && this <= 'Z\uffff');
};


String.prototype.isDigit = function () {
    return (this >= '0' && this <= '9');
};


// We build the application inside a function so that we produce only a single
// global variable. The function will be invoked, its return value is the JSLint
// function itself.

var jslint;
jslint = function () {

    var anonname,

// browser contains a set of global names which are commonly provided by a
// web browser environment.

        browser = {
            alert: true,
            blur: true,
            clearInterval: true,
            clearTimeout: true,
            close: true,
            closed: true,
            confirm: true,
            defaultStatus: true,
            document: true,
            event: true,
            focus: true,
            frames: true,
            history: true,
            Image: true,
            length: true,
            location: true,
            moveBy: true,
            moveTo: true,
            name: true,
            navigator: true,
            onblur: true,
            onerror: true,
            onfocus: true,
            onload: true,
            onresize: true,
            onunload: true,
            open: true,
            opener: true,
            parent: true,
            print: true,
            prompt: true,
            resizeBy: true,
            resizeTo: true,
            screen: true,
            scroll: true,
            scrollBy: true,
            scrollTo: true,
            self: true,
            setInterval: true,
            setTimeout: true,
            status: true,
            top: true,
            window: true,
            XMLHttpRequest: true
        },
        funlab, funstack, functions, globals,

// konfab contains the global names which are provided to a Konfabulator widget.

        konfab = {
            alert: true,
            animator: true,
            appleScript: true,
            beep: true,
            bytesToUIString: true,
            chooseColor: true,
            chooseFile: true,
            chooseFolder: true,
            convertPathToHFS: true,
            convertPathToPlatform: true,
            closeWidget: true,
            CustomAnimation: true,
            escape: true,
            FadeAnimation: true,
            focusWidget: true,
            form: true,
            include: true,
            isApplicationRunning: true,
            iTunes: true,
            konfabulatorVersion: true,
            log: true,
            MoveAnimation: true,
            openURL: true,
            play: true,
            popupMenu: true,
            print: true,
            prompt: true,
            reloadWidget: true,
            resolvePath: true,
            resumeUpdates: true,
            RotateAnimation: true,
            runCommand: true,
            runCommandInBg: true,
            saveAs: true,
            savePreferences: true,
            showWidgetPreferences: true,
            sleep: true,
            speak: true,
            suppressUpdates: true,
            tellWidget: true,
            unescape: true,
            updateNow: true,
            yahooCheckLogin: true,
            yahooLogin: true,
            yahooLogout: true,
            COM: true,
            filesystem: true,
            preferenceGroups: true,
            preferences: true,
            screen: true,
            system: true,
            URL: true,
            XMLDOM: true,
            XMLHttpRequest: true
        },
        lines, lookahead, member, noreach, option, prevtoken, stack,

// standard contains the global names that are provided by standard JavaScript.

        standard = {
            Array: true,
            Boolean: true,
            Date: true,
            decodeURI: true,
            decodeURIComponent: true,
            encodeURI: true,
            encodeURIComponent: true,
            Error: true,
            escape: true,
            eval: true,
            EvalError: true,
            Function: true,
            isFinite: true,
            isNaN: true,
            Math: true,
            Number: true,
            Object: true,
            parseInt: true,
            parseFloat: true,
            RangeError: true,
            ReferenceError: true,
            RegExp: true,
            String: true,
            SyntaxError: true,
            TypeError: true,
            unescape: true,
            URIError: true
        },
        syntax = {}, token, verb,
/*
    xmode is used to adapt to the exceptions in XML parsing. It can have these
    states:
        false   .js script file
        "       A " attribute
        '       A ' attribute
        content The content of a script tag
        CDATA   A CDATA block
*/
        xmode,
/*
    xtype identifies the type of document being analyzed. It can have these
    states:
        false   .js script file
        html    .html file
        widget  .kon Konfabulator file
*/
        xtype,
// token
        tx = /^([(){}[.,:;'"~]|\](\]>)?|\?>?|==?=?|\/(\*(global|extern)*|=|)|\*[\/=]?|\+[+=]?|-[-=]?|%[=>]?|&[&=]?|\|[|=]?|>>?>?=?|<([\/=%\?]|\!(\[|--)?|<=?)?|\^=?|\!=?=?|[a-zA-Z_$][a-zA-Z0-9_$]*|[0-9]+([xX][0-9a-fA-F]+|\.[0-9]*)?([eE][+-]?[0-9]+)?)/,
// string ending in single quote
        sx = /^((\\[^\x00-\x1f]|[^\x00-\x1f'\\])*)'/,
        sxx = /^(([^\x00-\x1f'])*)'/,
// string ending in double quote
        qx = /^((\\[^\x00-\x1f]|[^\x00-\x1f"\\])*)"/,
        qxx = /^(([^\x00-\x1f"])*)"/,
// regular expression
        rx = /^(\\[^\x00-\x1f]|\[(\\[^\x00-\x1f]|[^\x00-\x1f\\\/])*\]|[^\x00-\x1f\\\/\[])+\/[gim]*/,
// star slash
        lx = /\*\/|\/\*/,
// global identifier
        gx = /^([a-zA-Z_$][a-zA-Z0-9_$]*)/,
// identifier
        ix = /^([a-zA-Z_$][a-zA-Z0-9_$]*$)/,
// global separators
        hx = /^[\x00-\x20,]*(\*\/)?/,
// whitespace
        wx = /^\s*(\/\/.*\r*$)?/;

// Make a new object that inherits from an existing object.

    function object(o) {
        function f() {}
        f.prototype = o;
        return new f();
    }

// Produce an error warning.

    function warning(m, x, y) {
        var l, c, t = typeof x === 'object' ? x : token;
        if (typeof x === 'number') {
            l = x;
            c = y || 0;
        } else {
            if (t.id === '(end)') {
                t = prevtoken;
            }
            l = t.line || 0;
            c = t.from || 0;
        }
        jslint.errors.push({
            id: '(error)',
            reason: m,
            evidence: lines[l] || '',
            line: l,
            character: c
        });
        if (option.passfail) {
            jslint.errors.push(null);
            throw null;
        }
    }

    function error(m, x, y) {
        warning(m, x, y);
        jslint.errors.push(null);
        throw null;
    }


// lexical analysis

    var lex = function () {
        var character, from, line, s;

// Private lex methods

        function nextLine() {
            line += 1;
            if (line >= lines.length) {
                return false;
            }
            character = 0;
            s = lines[line];
            return true;
        }

// Produce a token object.  The token inherits from a syntax symbol.

        function it(type, value) {
            var t;
            if (type === '(punctuator)') {
                t = syntax[value];
            } else if (type === '(identifier)') {
                t = syntax[value];
                if (!t || typeof t != 'object') {
                    t = syntax[type];
                }
            } else {
                t = syntax[type];
            }
            if (!t || typeof t != 'object') {
                error("Unrecognized symbol: '" + value + "' " + type);
            }
            t = object(t);
            if (value || type === '(string)') {
                t.value = value;
            }
            t.line = line;
            t.character = character;
            t.from = from;
            return t;
        }

// Public lex methods

        return {
            init: function (source) {
                if (typeof source === 'string') {
                    lines = source.split('\n');
                    if (lines.length == 1) {
                        lines = lines[0].split('\r');
                    }
                } else {
                    lines = source;
                }
                line = 0;
                character = 0;
                from = 0;
                s = lines[0];
            },

// token -- this is called by advance to get the next token.

            token: function () {
                var c, i, l, r, t;

                function string(x) {
                    r = x.exec(s);
                    if (r) {
                        t = r[1];
                        l = r[0].length;
                        s = s.substr(l);
                        character += l;
                        if (xmode == 'script') {
                            if (t.indexOf('<\/') >= 0) {
                                warning(
    'Expected "...<\\/..." and instead saw "...<\/...".', token);
                            }
                        }
                        return it('(string)', r[1]);
                    } else {
                        for (var j = 0; j < s.length; j += 1) {
                            var c = s.charAt(j);
                            if (c < ' ') {
                                if (c === '\n' || c === '\r') {
                                    break;
                                }
                                error("Control character in string: " +
                                    s.substring(0, j), line, character + j);
                            }
                        }
                        error("Unclosed string: " + s, line, character);
                    }
                }

                for (;;) {
                    if (!s) {
                        return it(nextLine() ? '(endline)' : '(end)', '');
                    }
                    r = wx.exec(s);
                    if (!r || !r[0]) {
                        break;
                    }
                    l = r[0].length;
                    s = s.substr(l);
                    character += l;
                    if (s) {
                        break;
                    }
                }
                from = character;
                r = tx.exec(s);
                if (r) {
                    t = r[0];
                    l = t.length;
                    s = s.substr(l);
                    character += l;
                    c = t.substr(0, 1);

//      identifier

                    if (c.isAlpha() || c === '_' || c === '$') {
                        return it('(identifier)', t);
                    }

//      number

                    if (c.isDigit()) {
                        if (token.id === '.') {
                            warning(
            "A decimal fraction should have a zero before the decimal point.",
                                token);
                        }
                        if (!isFinite(Number(t))) {
                            warning("Bad number: '" + t + "'.",
                                line, character);
                        }
                        if (s.substr(0, 1).isAlpha()) {
                            error("Space is required after a number: '" +
                                t + "'.", line, character);
                        }
                        if (c === '0' && t.substr(1,1).isDigit()) {
                            warning("Don't use extra leading zeros: '" +
                                t + "'.", line, character);
                        }
                        if (t.substr(t.length - 1) === '.') {
                            warning(
    "A trailing decimal point can be confused with a dot: '" + t + "'.",
                                line, character);
                        }
                        return it('(number)', t);
                    }

//      string

                    if (t === '"') {
                        return (xmode === '"' ||  xmode === 'string') ?
                            it('(punctuator)', t) :
                            string(xmode === 'xml' ? qxx : qx);
                    }
                    if (t === "'") {
                        return (xmode === "'" ||  xmode === 'string') ?
                            it('(punctuator)', t) :
                            string(xmode === 'xml' ? sxx : sx);
                    }

//      unbegun comment

                    if (t === '/*') {
                        for (;;) {
                            i = s.search(lx);
                            if (i >= 0) {
                                break;
                            }
                            if (!nextLine()) {
                                error("Unclosed comment.", token);
                            }
                        }
                        character += i + 2;
                        if (s.substr(i, 1) === '/') {
                            error("Nested comment.");
                        }
                        s = s.substr(i + 2);
                        return this.token();
                    }

//      /*extern

                    if (t === '/*extern' || t === '/*global') {
                        for (;;) {
                            r = hx.exec(s);
                            if (r) {
                                l = r[0].length;
                                s = s.substr(l);
                                character += l;
                                if (r[1] === '*/') {
                                    return this.token();
                                }
                            }
                            if (s) {
                                r = gx.exec(s);
                                if (r) {
                                    l = r[0].length;
                                    s = s.substr(l);
                                    character += l;
                                    globals[r[1]] = true;
                                } else {
                                    error("Bad extern identifier: '" +
                                        s + "'.", line, character);
                                }
                             } else if (!nextLine()) {
                                error("Unclosed comment.");
                            }
                        }
                    }

//      punctuator

                    return it('(punctuator)', t);
                }
                error("Unexpected token: " + (t || s.substr(0, 1)),
                    line, character);
            },

// skip -- skip past the next occurrence of a particular string.
// If the argument is empty, skip to just before the next '<' character.
// This is used to ignore HTML content. Return false if it isn't found.

            skip: function (to) {
                if (token.id) {
                    if (!to) {
                        to = '';
                        if (token.id.substr(0, 1) === '<') {
                            lookahead.push(token);
                            return true;
                        }
                    } else if (token.id.indexOf(to) >= 0) {
                        return true;
                    }
                }
                prevtoken = token;
                token = syntax['(error)'];
                for (;;) {
                    var i = s.indexOf(to || '<');
                    if (i >= 0) {
                        character += i + to.length;
                        s = s.substr(i + to.length);
                        return true;
                    }
                    if (!nextLine()) {
                        break;
                    }
                }
                return false;
            },

// regex -- this is called by parse when it sees '/' being used as a prefix.

            regex: function () {
                var l, r = rx.exec(s), x;
                if (r) {
                    l = r[0].length;
                    character += l;
                    s = s.substr(l);
                    x = r[1];
                    return it('(regex)', x);
                }
                error("Bad regular expression: " + s);
            }
        };
    }();

    function builtin(name) {
        return standard[name] === true ||
               globals[name] === true ||
               (xtype === 'widget' && konfab[name] === true) ||
               ((xtype === 'html' || option.browser) && browser[name] === true);
    }

    function addlabel(t, type) {
        if (t) {
            if (typeof funlab[t] === 'string') {
                switch (funlab[t]) {
                case 'var':
                case 'var*':
                    if (type === 'global') {
                        funlab[t] = 'var*';
                        return;
                    }
                    break;
                case 'global':
                    if (type === 'var') {
                        warning('Var ' + t +
                            ' was used before it was declared.', prevtoken);
                    }
                    if (type === 'var*' || type === 'global') {
                        return;
                    }
                    break;
                case 'function':
                case 'parameter':
                    if (type === 'global') {
                        return;
                    }
                    break;
                }
                warning("Identifier '" + t + "' already declared as " +
                    funlab[t], prevtoken);
            }
            funlab[t] = type;
        }
    }


// We need a peek function. If it has an argument, it peeks that much farther
// ahead. It is used to distinguish
//     for ( var i in ...
// from
//     for ( var i = ...

    function peek(i) {
        var j = 0, t;
        if (token == syntax['(error)']) {
            return token;
        }
        if (typeof i === 'undefined') {
            i = 0;
        }
        while (j <= i) {
            t = lookahead[j];
            if (!t) {
                t = lookahead[j] = lex.token();
            }
            j += 1;
        }
        return t;
    }


    var badbreak = {')': true, ']': true, '++': true, '--': true};

// Produce the next token. It looks for programming errors.

    function advance(id, t) {
        var l;
        switch (prevtoken.id) {
        case '(number)':
            if (token.id === '.') {
                warning(
"A dot following a number can be confused with a decimal point.", prevtoken);
            }
            break;
        case '-':
            if (token.id === '-' || token.id === '--') {
                warning("Confusing minusses.");
            }
            break;
        case '+':
            if (token.id === '+' || token.id === '++') {
                warning("Confusing plusses.");
            }
            break;
        }
        if (prevtoken.type === '(string)' || prevtoken.identifier) {
            anonname = prevtoken.value;
        }

        if (id && token.value != id) {
            if (t) {
                if (token.id === '(end)') {
                    warning("Unmatched '" + t.id + "'.", t);
                } else {
                    warning("Expected '" + id + "' to match '" +
                        t.id + "' from line " + (t.line + 1) +
                        " and instead saw '" + token.value + "'.");
                }
            } else {
                warning("Expected '" + id + "' and instead saw '" +
                    token.value + "'.");
            }
        }
        prevtoken = token;
        for (;;) {
            token = lookahead.shift() || lex.token();
            if (token.id === '<![') {
                if (xtype === 'html') {
                    error("Unexpected token '<!['");
                }
                if (xmode === 'script') {
                    token = lex.token();
                    if (token.value !== 'CDATA') {
                        error("Expected 'CDATA'");
                    }
                    token = lex.token();
                    if (token.id !== '[') {
                        error("Expected '['");
                    }
                    xmode = 'CDATA';
                } else if (xmode === 'xml') {
                    lex.skip(']]>');
                } else {
                    error("Unexpected token '<!['");
                }
            } else if (token.id === ']]>') {
                if (xmode === 'CDATA') {
                    xmode = 'script';
                } else {
                    error("Unexpected token ']]>");
                }
            } else if (token.id !== '(endline)') {
                break;
            }
            if (xmode === '"' || xmode === "'") {
                error("Missing '" + xmode + "'.", prevtoken);
            }
            l = !xmode && !option.laxLineEnd &&
                (prevtoken.type == '(string)' || prevtoken.type == '(number)' ||
                prevtoken.type == '(identifier)' || badbreak[prevtoken.id]);
        }
        if (l && token.id != '{' && token.id != '}' && token.id != ']') {
            warning(
                "Strict line ending error: '" +
                prevtoken.value + "'.", prevtoken);
        }
        if (xtype === 'widget' && xmode === 'script' && token.id) {
            l = token.id.charAt(0);
            if (l === '<' || l === '&') {
                token.nud = token.led = null;
                token.lbp = 0;
                token.reach = true;
            }
        }
    }


    function advanceregex() {
        token = lex.regex();
    }


    function beginfunction(i) {
        var f = {'(name)': i, '(line)': token.line + 1, '(context)': funlab};
        funstack.push(funlab);
        funlab = f;
        functions.push(funlab);
    }


    function endfunction() {
        funlab = funstack.pop();
    }


// This is the heart of JSLint, the Pratt parser. In addition to parsing, it
// is looking for ad hoc lint patterns. We add to Pratt's model .fud, which is
// like nud except that it is only used on the first token of a statement.
// Having .fud makes it much easier to define JavaScript. I retained Pratt's
// nomenclature, even though it isn't very descriptive.

// .nud     Null denotation
// .fud     First null denotation
// .led     Left denotation
//  lbp     Left binding power
//  rbp     Right binding power

// They are key to the parsing method called Top Down Operator Precedence.

    function parse(rbp, initial) {
        var l, left, o;
        if (token.id && token.id === '/') {
            if (prevtoken.id != '(' && prevtoken.id != '=' &&
                    prevtoken.id != ':' && prevtoken.id != ',' &&
                    prevtoken.id != '=') {
                warning(
"Expected to see a '(' or '=' or ':' or ',' preceding a regular expression literal, and instead saw '" +
                    prevtoken.value + "'.", prevtoken);
            }
            advanceregex();
        }
        if (token.id === '(end)') {
            warning("Unexpected early end of program", prevtoken);
        }
        advance();
        if (initial) {
            anonname = 'anonymous';
            verb = prevtoken.value;
        }
        if (initial && prevtoken.fud) {
            prevtoken.fud();
        } else {
            if (prevtoken.nud) {
                o = prevtoken.exps;
                left = prevtoken.nud();
            } else {
                if (token.type === '(number)' && prevtoken.id === '.') {
                    warning(
"A leading decimal point can be confused with a dot: ." + token.value,
                        prevtoken);
                }
                error("Expected an identifier and instead saw '" +
                    prevtoken.id + "'.", prevtoken);
            }
            while (rbp < token.lbp) {
                o = token.exps;
                advance();
                if (prevtoken.led) {
                    left = prevtoken.led(left);
                } else {
                    error("Expected an operator and instead saw '" +
                        prevtoken.id + "'.");
                }
            }
            if (initial && !o) {
                warning(
"Expected an assignment or function call and instead saw an expression.",
                    prevtoken);
            }
        }
        if (l) {
            funlab[l] = 'label';
        }
        return left;
    }


// Parasitic constructors for making the symbols that will be inherited by
// tokens.

    function symbol(s, p) {
        return syntax[s] || (syntax[s] = {id: s, lbp: p, value: s});
    }


    function delim(s) {
        return symbol(s, 0);
    }


    function stmt(s, f) {
        var x = delim(s);
        x.identifier = x.reserved = true;
        x.fud = f;
        return x;
    }


    function blockstmt(s, f) {
        var x = stmt(s, f);
        x.block = true;
        return x;
    }


    function prefix(s, f) {
        var x = symbol(s, 150);
        x.nud = (typeof f === 'function') ? f : function () {
            parse(150);
            return this;
        };
        return x;
    }


    function prefixname(s, f) {
        var x = prefix(s, f);
        x.identifier = x.reserved = true;
        return x;
    }


    function type(s, f) {
        var x = delim(s);
        x.type = s;
        x.nud = f;
        return x;
    }


    function reserve(s, f) {
        var x = type(s, f);
        x.identifier = x.reserved = true;
        return x;
    }


    function reservevar(s) {
        return reserve(s, function () {
            return this;
        });
    }


    function infix(s, f, p) {
        var x = symbol(s, p);
        x.led = (typeof f === 'function') ? f : function (left) {
            return [f, left, parse(p)];
        };
        return x;
    }


    function assignop(s, f) {
        symbol(s, 20).exps = true;
        return infix(s, function (left) {
            if (left) {
                if (left.id === '.' || left.id === '[' ||
                        (left.identifier && !left.reserved)) {
                    parse(19);
                    return left;
                }
                if (left == syntax['function']) {
                    if (option.jscript) {
                        parse(19);
                        return left;
                    } else {
                        warning(
"Expected an identifier in an assignment, and instead saw a function invocation.",
                            prevtoken);
                    }
                }
            }
            error("Bad assignment.", this);
        }, 20);
    }


    function suffix(s, f) {
        var x = symbol(s, 150);
        x.led = function (left) {
            if (option.plusplus) {
                warning(this.id + " is considered harmful.", this);
            }
            return [f, left];
        };
        return x;
    }


    function optionalidentifier() {
        if (token.reserved) {
            warning("Expected an identifier and instead saw '" +
                token.id + "' (a reserved word).");
        }
        if (token.identifier) {
            advance();
            return prevtoken.value;
        }
    }


    function identifier() {
        var i = optionalidentifier();
        if (i) {
            return i;
        }
        if (prevtoken.id === 'function' && token.id === '(') {
            warning("Missing name in function statement.");
        } else {
            error("Expected an identifier and instead saw '" +
                    token.value + "'.", token);
        }
    }


    function reachable(s) {
        var i = 0, t;
        if (token.id != ';' || noreach) {
            return;
        }
        for (;;) {
            t = peek(i);
            if (t.reach) {
                return;
            }
            if (t.id != '(endline)') {
                if (t.id === 'function') {
                    warning(
"Inner functions should be listed at the top of the outer function.", t);
                    break;
                }
                warning("Unreachable '" + t.value + "' after '" + s +
                    "'.", t);
                break;
            }
            i += 1;
        }
    }


    function statement() {
        var t = token;
        while (t.id === ';') {
            warning("Unnecessary semicolon", t);
            advance(';');
            t = token;
            if (t.id === '}') {
                return;
            }
        }
        if (t.identifier && !t.reserved && peek().id === ':') {
            advance();
            advance(':');
            addlabel(t.value, 'live*');
            if (!token.labelled) {
                warning("Label '" + t.value +
                    "' on unlabelable statement '" + token.value + "'.",
                    token);
            }
            if (t.value.toLowerCase() == 'javascript') {
                warning("Label '" + t.value +
                    "' looks like a javascript url.",
                    token);
            }
            token.label = t.value;
            t = token;
        }
        parse(0, true);
        if (!t.block) {
            if (token.id != ';') {
                warning("Missing ';'", prevtoken.line,
                    prevtoken.from + prevtoken.value.length);
            } else {
                advance(';');
            }
        }
    }


    function statements() {
        while (!token.reach) {
            statement();
        }
    }


    function block() {
        var t = token;
        if (token.id === '{') {
            advance('{');
            statements();
            advance('}', t);
        } else {
            warning("Missing '{' before '" + token.value + "'.");
            noreach = true;
            statement();
            noreach = false;
        }
        verb = null;
    }


// An identity function, used by string and number tokens.

    function idValue() {
        return this;
    }


    function countMember(m) {
        if (typeof member[m] === 'number') {
            member[m] += 1;
        } else {
            member[m] = 1;
        }
    }


// Common HTML attributes that carry scripts.

    var scriptstring = {
        onblur:      true,
        onchange:    true,
        onclick:     true,
        ondblclick:  true,
        onfocus:     true,
        onkeydown:   true,
        onkeypress:  true,
        onkeyup:     true,
        onload:      true,
        onmousedown: true,
        onmousemove: true,
        onmouseout:  true,
        onmouseover: true,
        onmouseup:   true,
        onreset:     true,
        onselect:    true,
        onsubmit:    true,
        onunload:    true
    };


// XML types. Currently we support html and widget.

    var xmltype = {
        HTML: {
            doBegin: function (n) {
                if (!option.cap) {
                    warning("HTML case error.");
                }
                xmltype.html.doBegin();
            }
        },
        html: {
            doBegin: function (n) {
                xtype = 'html';
                xmltype.html.script = false;
            },
            doTagName: function (n, p) {
                var i, t = xmltype.html.tag[n], x;
                if (!t) {
                    error('Unrecognized tag: <' + n + '>. ' +
                        (n === n.toLowerCase() ?
                        'Did you mean <' + n.toLowerCase() + '>?' : ''));
                }
                x = t.parent;
                if (x) {
                    if (x.indexOf(' ' + p + ' ') < 0) {
                        error('A <' + n + '> must be within <' + x + '>',
                            prevtoken);
                    }
                } else {
                    i = stack.length;
                    do {
                        if (i <= 0) {
                            error('A <' + n + '> must be within the body',
                                prevtoken);
                        }
                        i -= 1;
                    } while (stack[i].name !== 'body');
                }
                xmltype.html.script = n === 'script';
                return t.simple;
            },
            doAttribute: function (n, a) {
                if (n === 'script') {
                    if (a === 'src') {
                        xmltype.html.script = false;
                        return 'string';
                    } else if (a === 'language') {
                        warning("The 'language' attribute is deprecated",
                            prevtoken);
                        return false;
                    }
                }
                return scriptstring[a] && 'script';
            },
            doIt: function (n) {
                return xmltype.html.script ? 'script' :
                    n !== 'html' && xmltype.html.tag[n].special && 'special';
            },
            tag: {
                a:        {},
                abbr:     {},
                acronym:  {},
                address:  {},
                applet:   {},
                area:     {simple: true, parent: ' map '},
                b:        {},
                base:     {simple: true, parent: ' head '},
                bdo:      {},
                big:      {},
                blockquote: {},
                body:     {parent: ' html noframes '},
                br:       {simple: true},
                button:   {},
                caption:  {parent: ' table '},
                center:   {},
                cite:     {},
                code:     {},
                col:      {simple: true, parent: ' table colgroup '},
                colgroup: {parent: ' table '},
                dd:       {parent: ' dl '},
                del:      {},
                dfn:      {},
                dir:      {},
                div:      {},
                dl:       {},
                dt:       {parent: ' dl '},
                em:       {},
                embed:    {},
                fieldset: {},
                font:     {},
                form:     {},
                frame:    {simple: true, parent: ' frameset '},
                frameset: {parent: ' html frameset '},
                h1:       {},
                h2:       {},
                h3:       {},
                h4:       {},
                h5:       {},
                h6:       {},
                head:     {parent: ' html '},
                html:     {},
                hr:       {simple: true},
                i:        {},
                iframe:   {},
                img:      {simple: true},
                input:    {simple: true},
                ins:      {},
                kbd:      {},
                label:    {},
                legend:   {parent: ' fieldset '},
                li:       {parent: ' dir menu ol ul '},
                link:     {simple: true, parent: ' head '},
                map:      {},
                menu:     {},
                meta:     {simple: true, parent: ' head noscript '},
                noframes: {parent: ' html body '},
                noscript: {parent: ' html head body frameset '},
                object:   {},
                ol:       {},
                optgroup: {parent: ' select '},
                option:   {parent: ' optgroup select '},
                p:        {},
                param:    {simple: true, parent: ' applet object '},
                pre:      {},
                q:        {},
                samp:     {},
                script:   {parent:
' head body p div span abbr acronym address bdo blockquote cite code del dfn em ins kbd pre samp strong th td var '},
                select:   {},
                small:    {},
                span:     {},
                strong:   {},
                style:    {parent: ' head ', special: true},
                sub:      {},
                sup:      {},
                table:    {},
                tbody:    {parent: ' table '},
                td:       {parent: ' tr '},
                textarea: {},
                tfoot:    {parent: ' table '},
                th:       {parent: ' tr '},
                thead:    {parent: ' table '},
                title:    {parent: ' head '},
                tr:       {parent: ' table tbody thead tfoot '},
                tt:       {},
                u:        {},
                ul:       {},
                'var':    {}
            }
        },
        widget: {
            doBegin: function (n) {
                xtype = 'widget';
            },
            doTagName: function (n, p) {
                var t = xmltype.widget.tag[n];
                if (!t) {
                    error('Unrecognized tag: <' + n + '>. ');
                }
                var x = t.parent;
                if (x.indexOf(' ' + p + ' ') < 0) {
                    error('A <' + n + '> must be within <' + x + '>', prevtoken);
                }
            },
            doAttribute: function (n, a) {
                var t = xmltype.widget.tag[a];
                if (!t) {
                    error('Unrecognized attribute: <' + n + ' ' + a + '>. ');
                }
                var x = t.parent;
                if (x.indexOf(' ' + n + ' ') < 0) {
                    error('Attribute ' + a + ' does not belong in <' +
                        n + '>');
                }
                return t.script ? 'script' : a === 'name' ? 'define' : 'string';
            },
            doIt: function (n) {
                var x = xmltype.widget.tag[n];
                return x && x.script && 'script';
            },
            tag: {
                "about-box": {parent: ' widget '},
                "about-image": {parent: ' about-box '},
                "about-text": {parent: ' about-box '},
                "about-version": {parent: ' about-box '},
                action: {parent: ' widget ', script: true},
                alignment: {parent: ' image text textarea window '},
                author: {parent: ' widget '},
                autoHide: {parent: ' scrollbar '},
                bgColor: {parent: ' text textarea '},
                bgOpacity: {parent: ' text textarea '},
                checked: {parent: ' image '},
                clipRect: {parent: ' image '},
                color: {parent: ' about-text about-version shadow text textarea '},
                contextMenuItems: {parent: ' frame image text textarea window '},
                colorize: {parent: ' image '},
                columns: {parent: ' textarea '},
                company: {parent: ' widget '},
                copyright: {parent: ' widget '},
                data: {parent: ' about-text about-version text textarea '},
                debug: {parent: ' widget '},
                defaultValue: {parent: ' preference '},
                defaultTracking: {parent: ' widget '},
                description: {parent: ' preference '},
                directory: {parent: ' preference '},
                editable: {parent: ' textarea '},
                enabled: {parent: ' menuItem '},
                extension: {parent: ' preference '},
                file: {parent: ' action preference '},
                fillMode: {parent: ' image '},
                font: {parent: ' about-text about-version text textarea '},
                frame: {parent: ' frame window '},
                group: {parent: ' preference '},
                hAlign: {parent: ' frame image scrollbar text textarea '},
                height: {parent: ' frame image scrollbar text textarea window '},
                hidden: {parent: ' preference '},
                hLineSize: {parent: ' frame '},
                hOffset: {parent: ' about-text about-version frame image scrollbar shadow text textarea window '},
                hotkey: {parent: ' widget '},
                hRegistrationPoint: {parent: ' image '},
                hslAdjustment: {parent: ' image '},
                hslTinting: {parent: ' image '},
                hScrollBar: {parent: ' frame '},
                icon: {parent: ' preferenceGroup '},
                image: {parent: ' about-box frame window widget '},
                interval: {parent: ' action timer '},
                key: {parent: ' hotkey '},
                kind: {parent: ' preference '},
                level: {parent: ' window '},
                lines: {parent: ' textarea '},
                loadingSrc: {parent: ' image '},
                max: {parent: ' scrollbar '},
                maxLength: {parent: ' preference '},
                menuItem: {parent: ' contextMenuItems '},
                min: {parent: ' scrollbar '},
                minimumVersion: {parent: ' widget '},
                minLength: {parent: ' preference '},
                missingSrc: {parent: ' image '},
                modifier: {parent: ' hotkey '},
                name: {parent: ' hotkey image preference preferenceGroup text textarea timer window '},
                notSaved: {parent: ' preference '},
                onContextMenu: {parent: ' frame image text textarea window ', script: true},
                onDragDrop: {parent: ' frame image text textarea ', script: true},
                onDragEnter: {parent: ' frame image text textarea ', script: true},
                onDragExit: {parent: ' frame image text textarea ', script: true},
                onFirstDisplay: {parent: ' window ', script: true},
                onGainFocus: {parent: ' textarea window ', script: true},
                onKeyDown: {parent: ' hotkey text textarea ', script: true},
                onKeyPress: {parent: ' textarea ', script: true},
                onKeyUp: {parent: ' hotkey text textarea ', script: true},
                onImageLoaded: {parent: ' image ', script: true},
                onLoseFocus: {parent: ' textarea window ', script: true},
                onMouseDown: {parent: ' frame image text textarea ', script: true},
                onMouseEnter: {parent: ' frame image text textarea ', script: true},
                onMouseExit: {parent: ' frame image text textarea ', script: true},
                onMouseMove: {parent: ' frame image text ', script: true},
                onMouseUp: {parent: ' frame image text textarea ', script: true},
                onMouseWheel: {parent: ' frame ', script: true},
                onMultiClick: {parent: ' frame image text textarea window ', script: true},
                onSelect: {parent: ' menuItem ', script: true},
                onTimerFired: {parent: ' timer ', script: true},
                onValueChanged: {parent: ' scrollbar ', script: true},
                opacity: {parent: ' frame image scrollbar shadow text textarea window '},
                option: {parent: ' preference widget '},
                optionValue: {parent: ' preference '},
                order: {parent: ' preferenceGroup '},
                orientation: {parent: ' scrollbar '},
                pageSize: {parent: ' scrollbar '},
                preference: {parent: ' widget '},
                preferenceGroup: {parent: ' widget '},
                remoteAsync: {parent: ' image '},
                requiredPlatform: {parent: ' widget '},
                rotation: {parent: ' image '},
                scrollX: {parent: ' frame '},
                scrollY: {parent: ' frame '},
                secure: {parent: ' preference textarea '},
                scrollbar: {parent: ' text textarea '},
                shadow: {parent: ' about-text text window '},
                size: {parent: ' about-text about-version text textarea '},
                spellcheck: {parent: ' textarea '},
                src: {parent: ' image '},
                srcHeight: {parent: ' image '},
                srcWidth: {parent: ' image '},
                style: {parent: ' about-text about-version preference text textarea '},
                text: {parent: ' frame window '},
                textarea: {parent: ' frame window '},
                timer: {parent: ' widget '},
                thumbColor: {parent: ' scrollbar '},
                ticking: {parent: ' timer '},
                ticks: {parent: ' preference '},
                tickLabel: {parent: ' preference '},
                tileOrigin: {parent: ' image '},
                title: {parent: ' menuItem preference preferenceGroup window '},
                tooltip: {parent: ' image text textarea '},
                truncation: {parent: ' text '},
                tracking: {parent: ' image '},
                trigger: {parent: ' action '},
                truncation: {parent: ' text textarea '},
                type: {parent: ' preference '},
                useFileIcon: {parent: ' image '},
                vAlign: {parent: ' frame image scrollbar text textarea '},
                value: {parent: ' preference scrollbar '},
                version: {parent: ' widget '},
                visible: {parent: ' frame image scrollbar text textarea window '},
                vLineSize: {parent: ' frame '},
                vOffset: {parent: ' about-text about-version frame image scrollbar shadow text textarea window '},
                vRegistrationPoint: {parent: ' image '},
                vScrollBar: {parent: ' frame '},
                width: {parent: ' frame image scrollbar text textarea window '},
                window: {parent: ' widget '},
                zOrder: {parent: ' frame image scrollbar text textarea '}
            }
        }
    };

    function xmlword(tag) {
        var w = token.value;
        if (!token.identifier) {
            if (token.id === '<') {
                error(tag ? "Expected &lt; and saw '<'" : "Missing '>'",
                    prevtoken);
            } else {
                warning("Missing quotes", prevtoken);
            }
        }
        advance();
        while (token.id === '-' || token.id === ':') {
            w += token.id;
            advance();
            if (!token.identifier) {
                error('Bad name: ' + w + token.value);
            }
            w += token.value;
            advance();
        }
        return w;
    }

    function xml() {
        var a, e, n, q, t;
        xmode = 'xml';
        stack = [];
        for (;;) {
            switch (token.value) {
            case '<':
                advance('<');
                t = token;
                n = xmlword(true);
                t.name = n;
                if (!xtype) {
                    if (xmltype[n]) {
                        xmltype[n].doBegin();
                        n = xtype;
                        e = false;
                    } else {
                        error("Unrecognized <" + n + ">");
                    }
                } else {
                    if (option.cap && xtype === 'html') {
                        n = n.toLowerCase();
                    }
                    e = xmltype[xtype].doTagName(n, stack[stack.length - 1].type);
                }
                t.type = n;
                for (;;) {
                    if (token.id === '/') {
                        advance('/');
                        e = true;
                        break;
                    }
                    if (token.id && token.id.substr(0, 1) === '>') {
                        break;
                    }
                    a = xmlword();
                    switch (xmltype[xtype].doAttribute(n, a)) {
                    case 'script':
                        xmode = 'string';
                        advance('=');
                        q = token.id;
                        if (q !== '"' && q !== "'") {
                            error('Missing quote.');
                        }
                        xmode = q;
                        advance(q);
                        statements();
                        if (token.id !== q) {
                            error(
                              'Missing close quote on script attribute');
                        }
                        xmode = 'xml';
                        advance(q);
                        break;
                    case 'value':
                        advance('=');
                        if (!token.identifier &&
                                token.type != '(string)' &&
                                token.type != '(number)') {
                            error('Bad value: ' + token.value);
                        }
                        advance();
                        break;
                    case 'string':
                        advance('=');
                        if (token.type !== '(string)') {
                            error('Bad value: ' + token.value);
                        }
                        advance();
                        break;
                    case 'define':
                        advance('=');
                        if (token.type !== '(string)') {
                            error('Bad value: ' + token.value);
                        }
                        addlabel(token.value, 'global');
                        advance();
                        break;
                    default:
                        if (token.id === '=') {
                            advance('=');
                            if (!token.identifier &&
                                    token.type != '(string)' &&
                                    token.type != '(number)') {
                            }
                            advance();
                        }
                    }
                }
                switch (xmltype[xtype].doIt(n)) {
                case 'script':
                    xmode = 'script';
                    advance('>');
                    statements();
                    xmode = 'xml';
                    break;
                case 'special':
                    e = true;
                    n = '</' + t.name + '>';
                    if (!lex.skip(n)) {
                        error("Missing " + n, t);
                    }
                    break;
                default:
                    lex.skip('>');
                }
                if (!e) {
                    stack.push(t);
                }
                break;
            case '</':
                advance('</');
                n = xmlword(true);
                t = stack.pop();
                if (!t) {
                    error('Unexpected close tag: </' + n + '>');
                }
                if (t.name != n) {
                    error('Expected </' + t.name +
                        '> and instead saw </' + n + '>');
                }
                if (token.id !== '>') {
                    error("Expected '>'");
                }
                lex.skip('>');
                break;
            case '<!':
                for (;;) {
                    advance();
                    if (token.id === '>') {
                        break;
                    }
                    if (token.id === '<' || token.id === '(end)') {
                        error("Missing '>'.", prevtoken);
                    }
                }
                lex.skip('>');
                break;
            case '<!--':
                lex.skip('-->');
                break;
            case '<%':
                lex.skip('%>');
                break;
            case '<?':
                for (;;) {
                    advance();
                    if (token.id === '?>') {
                        break;
                    }
                    if (token.id === '<?' || token.id === '<' ||
                            token.id === '>' || token.id === '(end)') {
                        error("Missing '?>'.", prevtoken);
                    }
                }
                lex.skip('?>');
                break;
            case '<=':
            case '<<':
            case '<<=':
                error("Expected '&lt;'.");
                break;
            case '(end)':
                return;
            }
            if (!lex.skip('')) {
                if (stack.length) {
                    t = stack.pop();
                    error('Missing </' + t.name + '>', t);
                }
                return;
            }
            advance();
        }
    }


// Build the syntax table by declaring the syntactic elements of the language.

    type('(number)', idValue);
    type('(string)', idValue);

    syntax['(identifier)'] = {
        type: '(identifier)',
        lbp: 0,
        identifier: true,
        nud: function () {
            if (option.undef && !builtin(this.value) &&
                    xmode !== '"' && xmode !== "'") {
                var c = funlab;
                while (!c[this.value]) {
                    c = c['(context)'];
                    if (!c) {
                        warning("Undefined variable: " + this.value,
                            prevtoken);
                        break;
                    }
                }
            }
            addlabel(this.value, 'global');
            return this;
        },
        led: function () {
            error("Expected an operator and instead saw '" +
                token.value + "'.");
        }
    };

    type('(regex)', function () {
        return [this.id, this.value, this.flags];
    });

    delim('(endline)');
    delim('(begin)');
    delim('(end)').reach = true;
    delim('</').reach = true;
    delim('<![').reach = true;
    delim('<%');
    delim('<?');
    delim('<!');
    delim('<!--');
    delim('%>');
    delim('?>');
    delim('(error)').reach = true;
    delim('}').reach = true;
    delim(')');
    delim(']');
    delim(']]>').reach = true;
    delim('"').reach = true;
    delim("'").reach = true;
    delim(';');
    delim(':').reach = true;
    delim(',');
    reserve('else');
    reserve('case').reach = true;
    reserve('default').reach = true;
    reserve('catch');
    reserve('finally');
    reservevar('arguments');
    reservevar('false');
    reservevar('Infinity');
    reservevar('NaN');
    reservevar('null');
    reservevar('this');
    reservevar('true');
    reservevar('undefined');
    assignop('=', 'assign', 20);
    assignop('+=', 'assignadd', 20);
    assignop('-=', 'assignsub', 20);
    assignop('*=', 'assignmult', 20);
    assignop('/=', 'assigndiv', 20).nud = function () {
        warning(
            "A regular expression literal can be confused with '/='.");
    };
    assignop('%=', 'assignmod', 20);
    assignop('&=', 'assignbitand', 20);
    assignop('|=', 'assignbitor', 20);
    assignop('^=', 'assignbitxor', 20);
    assignop('<<=', 'assignshiftleft', 20);
    assignop('>>=', 'assignshiftright', 20);
    assignop('>>>=', 'assignshiftrightunsigned', 20);
    infix('?', function (left) {
        parse(10);
        advance(':');
        parse(10);
    }, 30);

    infix('||', 'or', 40);
    infix('&&', 'and', 50);
    infix('|', 'bitor', 70);
    infix('^', 'bitxor', 80);
    infix('&', 'bitand', 90);
    infix('==', function (left) {
        var t = token;
        if (    (t.type === '(number)' && !+t.value) ||
                (t.type === '(string)' && !t.value) ||
                t.type === 'true' || t.type === 'false' ||
                t.type === 'undefined' || t.type === 'null') {
            warning("Use '===' to compare with '" + t.value + "'.", t);
        }
        return ['==', left, parse(100)];
    }, 100);
    infix('===', 'equalexact', 100);
    infix('!=', function (left) {
        var t = token;
        if (    (t.type === '(number)' && !+t.value) ||
                (t.type === '(string)' && !t.value) ||
                t.type === 'true' || t.type === 'false' ||
                t.type === 'undefined' || t.type === 'null') {
            warning("Use '!==' to compare with '" + t.value + "'.", t);
        }
        return ['!=', left, parse(100)];
    }, 100);
    infix('!==', 'notequalexact', 100);
    infix('<', 'less', 110);
    infix('>', 'greater', 110);
    infix('<=', 'lessequal', 110);
    infix('>=', 'greaterequal', 110);
    infix('<<', 'shiftleft', 120);
    infix('>>', 'shiftright', 120);
    infix('>>>', 'shiftrightunsigned', 120);
    infix('in', 'in', 120);
    infix('instanceof', 'instanceof', 120);
    infix('+', 'addconcat', 130);
    prefix('+', 'num');
    infix('-', 'sub', 130);
    prefix('-', 'neg');
    infix('*', 'mult', 140);
    infix('/', 'div', 140);
    infix('%', 'mod', 140);

    suffix('++', 'postinc');
    prefix('++', 'preinc');
    syntax['++'].exps = true;

    suffix('--', 'postdec');
    prefix('--', 'predec');
    syntax['--'].exps = true;
    prefixname('delete', function () {
        parse(0);
    }).exps = true;


    prefix('~', 'bitnot');
    prefix('!', 'not');
    prefixname('typeof', 'typeof');
    prefixname('new', function () {
        var c = parse(155);
        if (c) {
            if (c.identifier) {
                switch (c.value) {
                case 'Object':
                    warning('Use the object literal notation {}.', prevtoken);
                    break;
                case 'Array':
                    warning('Use the array literal notation [].', prevtoken);
                    break;
                case 'Number':
                case 'String':
                case 'Boolean':
                    warning("Do not use the " + c.value +
                        " function as a constructor.", prevtoken);
                    break;
                case 'Function':
                    if (!option.evil) {
                        warning('The Function constructor is eval.');
                    }
                }
            } else {
                if (c.id !== '.' && c.id !== '[' && c.id !== '(') {
                    warning('Bad constructor', prevtoken);
                }
            }
        } else {
            warning('Weird construction.', this);
        }
        if (token.id === '(') {
            advance('(');
            if (token.id !== ')') {
                for (;;) {
                    parse(10);
                    if (token.id !== ',') {
                        break;
                    }
                    advance(',');
                }
            }
            advance(')');
        } else {
            warning("Missing '()' invoking a constructor.");
        }
        return syntax['function'];
    });
    syntax['new'].exps = true;

    infix('.', function (left) {
        var m = identifier();
        if (typeof m === 'string') {
            countMember(m);
        }
        if (!option.evil && left && left.value === 'document' &&
                (m === 'write' || m === 'writeln')) {
            warning("document.write can be a form of eval.", left);
        }
        this.left = left;
        this.right = m;
        return this;
    }, 160);

    infix('(', function (left) {
        var n = 0, p = [];
        if (token.id !== ')') {
            for (;;) {
                p[p.length] = parse(10);
                n += 1;
                if (token.id !== ',') {
                    break;
                }
                advance(',');
            }
        }
        advance(')');
        if (typeof left === 'object') {
            if (left.value == 'parseInt' && n == 1) {
                warning("Missing radix parameter", left);
            }
            if (!option.evil) {
                if (left.value == 'eval' || left.value == 'Function') {
                    warning("eval is evil", left);
                } else if (p[0] && p[0].id === '(string)' &&
                       (left.value === 'setTimeout' ||
                        left.value === 'setInterval')) {
                    warning(
    "Implied eval is evil. Use a function argument instead of a string", left);
                }
            }
            if (!left.identifier && left.id !== '.' &&
                    left.id !== '[' && left.id !== '(') {
                warning('Bad invocation.', left);
            }

        }
        return syntax['function'];
    }, 155).exps = true;

    prefix('(', function () {
        parse(0);
        advance(')', this);
    });

    infix('[', function (left) {
        var e = parse(0);
        if (e && e.type === '(string)') {
            countMember(e.value);
            if (ix.test(e.value)) {
                var s = syntax[e.value];
                if (!s || !s.reserved) {
                    warning("This is better written in dot notation.", e);
                }
            }
        }
        advance(']', this);
        this.left = left;
        this.right = e;
        return this;
    }, 160);

    prefix('[', function () {
        if (token.id === ']') {
            advance(']');
            return;
        }
        for (;;) {
            parse(10);
            if (token.id === ',') {
                advance(',');
                if (token.id === ']' || token.id === ',') {
                    warning('Extra comma.', prevtoken);
                }
            } else {
                advance(']', this);
                return;
            }
        }
    }, 160);

    (function (x) {
        x.nud = function () {
            var i;
            if (token.id === '}') {
                advance('}');
                return;
            }
            for (;;) {
                i = optionalidentifier(true);
                if (!i && (token.id === '(string)' || token.id === '(number)')) {
                    i = token.id;
                    advance();
                }
                if (!i) {
                    error("Expected an identifier and instead saw '" +
                        token.value + "'.");
                }
                if (typeof i.value === 'string') {
                    countMember(i.value);
                }
                advance(':');
                parse(10);
                if (token.id === ',') {
                    advance(',');
                    if (token.id === ',' || token.id === '}') {
                        warning("Extra comma.");
                    }
                } else {
                    advance('}', this);
                    return;
                }
            }
        };
        x.fud = function () {
            error(
                "Expected to see a statement and instead saw a block.");
        };
    })(delim('{'));


    function varstatement() {
        for (;;) {
            addlabel(identifier(), 'var');
            if (token.id === '=') {
                advance('=');
                parse(20);
            }
            if (token.id === ',') {
                advance(',');
            } else {
                return;
            }
        }
    }


    stmt('var', varstatement);


    function functionparams() {
        var t = token;
        advance('(');
        if (token.id === ')') {
            advance(')');
            return;
        }
        for (;;) {
            addlabel(identifier(), 'parameter');
            if (token.id === ',') {
                advance(',');
            } else {
                advance(')', t);
                return;
            }
        }
    }


    blockstmt('function', function () {
        var i = identifier();
        addlabel(i, 'var*');
        beginfunction(i);
        addlabel(i, 'function');
        functionparams();
        block();
        endfunction();
    });

    prefixname('function', function () {
        var i = optionalidentifier() || ('"' + anonname + '"');
        beginfunction(i);
        addlabel(i, 'function');
        functionparams();
        block();
        endfunction();
    });

    blockstmt('if', function () {
        var t = token;
        advance('(');
        parse(20);
        advance(')', t);
        block();
        if (token.id === 'else') {
            advance('else');
            if (token.id === 'if' || token.id === 'switch') {
                statement();
            } else {
                block();
            }
        }
    });

    blockstmt('try', function () {
        var b;
        block();
        if (token.id === 'catch') {
            advance('catch');
            beginfunction('"catch"');
            functionparams();
            block();
            endfunction();
            b = true;
        }
        if (token.id === 'finally') {
            advance('finally');
            beginfunction('"finally"');
            block();
            endfunction();
            return;
        } else if (!b) {
            error("Expected 'catch' or 'finally' and instead saw '" +
                token.value + "'.");
        }
    });

    blockstmt('while', function () {
        var t= token;
        advance('(');
        parse(20);
        advance(')', t);
        block();
    }).labelled = true;

    reserve('with');

    blockstmt('switch', function () {
        var t = token;
        advance('(');
        var g = false;
        parse(20);
        advance(')', t);
        t = token;
        advance('{');
        for (;;) {
            switch (token.id) {
            case 'case':
                switch (verb) {
                case 'break':
                case 'case':
                case 'continue':
                case 'return':
                case 'switch':
                case 'throw':
                    break;
                default:
                    warning(
                        "Expected a 'break' statement before 'case'.",
                        prevtoken);
                }
                advance('case');
                parse(20);
                g = true;
                advance(':');
                verb = 'case';
                break;
            case 'default':
                switch (verb) {
                case 'break':
                case 'continue':
                case 'return':
                case 'throw':
                    break;
                default:
                    warning(
                        "Expected a 'break' statement before 'default'.",
                        prevtoken);
                }
                advance('default');
                g = true;
                advance(':');
                break;
            case '}':
                advance('}', t);
                return;
            default:
                if (g) {
                    statements();
                } else {
                    error("Expected to see 'case' and instead saw '" +
                        token.value + "'.");
                }
            }
        }
    }).labelled = true;

    stmt('debugger', function () {
        if (!option.debug) {
            warning("All debugger statements should be removed.");
        }
    });

    stmt('do', function () {
        block();
        advance('while');
        var t = token;
        advance('(');
        parse(20);
        advance(')', t);
    }).labelled = true;

    blockstmt('for', function () {
        var t = token;
        advance('(');
        if (peek(token.id === 'var' ? 1 : 0).id === 'in') {
            if (token.id === 'var') {
                advance('var');
                addlabel(identifier(), 'var');
            } else {
                advance();
            }
            advance('in');
            parse(20);
            advance(')', t);
            block();
            return;
        } else {
            if (token.id != ';') {
                if (token.id === 'var') {
                    advance('var');
                    varstatement();
                } else {
                    for (;;) {
                        parse(0);
                        if (token.id !== ',') {
                            break;
                        }
                        advance(',');
                    }
                }
            }
            advance(';');
            if (token.id != ';') {
                parse(20);
            }
            advance(';');
            if (token.id === ';') {
                error("Expected to see ')' and instead saw ';'");
            }
            if (token.id != ')') {
                for (;;) {
                    parse(0);
                    if (token.id !== ',') {
                        break;
                    }
                    advance(',');
                }
            }
            advance(')', t);
            block();
        }
    }).labelled = true;


    function nolinebreak(t) {
        if (t.line !== token.line) {
            warning("Statement broken badly.", t);
        }
    }


    stmt('break', function () {
        nolinebreak(this);
        if (funlab[token.value] === 'live*') {
            advance();
        }
        reachable('break');
    });


    stmt('continue', function () {
        nolinebreak(this);
        if (funlab[token.id] === 'live*') {
            advance();
        }
        reachable('continue');
    });


    stmt('return', function () {
        nolinebreak(this);
        if (token.id != ';' && !token.reach) {
            parse(20);
        }
        reachable('return');
    });


    stmt('throw', function () {
        nolinebreak(this);
        parse(20);
        reachable('throw');
    });


//  Superfluous reserved words

    reserve('abstract');
    reserve('boolean');
    reserve('byte');
    reserve('char');
    reserve('class');
    reserve('const');
    reserve('double');
    reserve('enum');
    reserve('export');
    reserve('extends');
    reserve('final');
    reserve('float');
    reserve('goto');
    reserve('implements');
    reserve('import');
    reserve('int');
    reserve('interface');
    reserve('long');
    reserve('native');
    reserve('package');
    reserve('private');
    reserve('protected');
    reserve('public');
    reserve('short');
    reserve('static');
    reserve('super');
    reserve('synchronized');
    reserve('throws');
    reserve('transient');
    reserve('void');
    reserve('volatile');


// The actual jslint function itself.

    var j = function (s, o) {
        option = o;
        if (!o) {
            option = {};
        }
        jslint.errors = [];
        globals = {};
        functions = [];
        xmode = false;
        xtype = '';
        stack = null;
        funlab = {};
        member = {};
        funstack = [];
        lookahead = [];
        lex.init(s);

        prevtoken = token = syntax['(begin)'];
        try {
            advance();
            if (token.value.charAt(0) === '<') {
                xml();
            } else {
                statements();
                advance('(end)');
            }
        } catch (e) {
            if (e) {
                jslint.errors.push({
                    reason: "JSLint error: " + e.description,
                    line: token.line,
                    character: token.from,
                    evidence: token.value
                });
            }
        }
        return jslint.errors.length === 0;
    };


// Report generator.

    j.report = function (option) {
        var a = [], c, cc, f, i, k, o = [], s;

        function detail(h) {
            if (s.length) {
                o.push('<div>' + h + ':&nbsp; ' + s.sort().join(', ') +
                    '</div>');
            }
        }

        k = jslint.errors.length;
        if (k) {
            o.push(
                '<div style="background-color: mistyrose;">Error:<blockquote>');
            for (i = 0; i < k; i += 1) {
                c = jslint.errors[i];
                if (c) {
                    o.push('<p>Problem at line ' + (c.line + 1) +
                        ' character ' + (c.character + 1) +
                        ': ' + c.reason.entityify() +
                        '</p><p><tt>' + c.evidence.entityify() +
                        '</tt></p>');
                }
            }
            o.push('</blockquote></div>');
            if (!c) {
                return o.join('');
            }
        }

        if (!option) {
            for (k in member) {
                a.push(k);
            }
            if (a.length) {
                a = a.sort();
                o.push(
                 '<table><tbody><tr><th>Members</th><th>Occurrences</th></tr>');
                for (i = 0; i < a.length; i += 1) {
                    o.push('<tr><td><tt>', a[i], '</tt></td><td>', member[a[i]],
                        '</td></tr>');
                }
                o.push('</tbody></table>');
            }
            for (i = 0; i < functions.length; ++i) {
                f = functions[i];
                for (k in f) {
                    if (f[k] === 'global') {
                        c = f['(context)'];
                        for (;;) {
                            cc = c['(context)'];
                            if (!cc) {
                                if ((!funlab[k] || funlab[k] === 'var?') &&
                                        !builtin(k)) {
                                    funlab[k] = 'var?';
                                    f[k] = 'global?';
                                }
                                break;
                            }
                            if (c[k] === 'parameter!' || c[k] === 'var!') {
                                f[k] = 'var.';
                                break;
                            }
                            if (c[k] === 'var' || c[k] === 'var*' ||
                                    c[k] === 'var!') {
                                f[k] = 'var.';
                                c[k] = 'var!';
                                break;
                            }
                            if (c[k] === 'parameter') {
                                f[k] = 'var.';
                                c[k] = 'parameter!';
                                break;
                            }
                            c = cc;
                        }
                    }
                }
            }
            s = [];
            for (k in funlab) {
                c = funlab[k];
                if (typeof c === 'string' && c.substr(0, 3) === 'var') {
                    if (c === 'var?') {
                        s.push('<tt>' + k + '</tt><small>&nbsp;(?)</small>');
                    } else {
                        s.push('<tt>' + k + '</tt>');
                    }
                }
            }
            detail('Global');
            if (functions.length) {
                o.push('<br>Function:<ol style="padding-left:0.5in">');
            }
            for (i = 0; i < functions.length; i += 1) {
                f = functions[i];
                o.push('<li value=' +
                    f['(line)'] + '><tt>' + (f['(name)'] || '') + '</tt>');
                s = [];
                for (k in f) {
                    if (k.charAt(0) != '(') {
                        switch (f[k]) {
                        case 'parameter':
                            s.push('<tt>' + k + '</tt>');
                            break;
                        case 'parameter!':
                            s.push('<tt>' + k +
                                '</tt><small>&nbsp;(closure)</small>');
                            break;
                        }
                    }
                }
                detail('Parameter');
                s = [];
                for (k in f) {
                    if (k.charAt(0) != '(') {
                        switch(f[k]) {
                        case 'var':
                            s.push('<tt>' + k +
                                '</tt><small>&nbsp;(unused)</small>');
                            break;
                        case 'var*':
                            s.push('<tt>' + k + '</tt>');
                            break;
                        case 'var!':
                            s.push('<tt>' + k +
                                '</tt><small>&nbsp;(closure)</small>');
                            break;
                        case 'var.':
                            s.push('<tt>' + k +
                                '</tt><small>&nbsp;(outer)</small>');
                            break;
                        }
                    }
                }
                detail('Var');
                s = [];
                c = f['(context)'];
                for (k in f) {
                    if (k.charAt(0) != '(' && f[k].substr(0, 6) === 'global') {
                        if (f[k] === 'global?') {
                            s.push('<tt>' + k + '</tt><small>&nbsp;(?)</small>');
                        } else {
                            s.push('<tt>' + k + '</tt>');
                        }
                    }
                }
                detail('Global');
                s = [];
                for (k in f) {
                    if (k.charAt(0) != '(' && f[k] === 'label') {
                       s.push(k);
                    }
                }
                detail('Label');
                o.push('</li>');
            }
            if (functions.length) {
                o.push('</ol>');
            }
        }
        return o.join('');
    };

    return j;

}();

/* ============================================================== */

var options = {
    "browser"    : false,
    "cap"        : false,
    "debug"      : false,
    "evil"       : false,
    "jscript"    : false,
    "laxLineEnd" : false,
    "passfail"   : false,
    "plusplus"   : false,
    "undef"      : false
};

function die(str) {
    print("jslint:ERROR: " + str);
    quit();
}

function usage() {
    print("jslint:USAGE: jslint file.js");
    quit();
}

var i;
for (i = 0; i < arguments.length; i++) {
    if (   arguments[i].substring(0, 1) != '-'
        && arguments[i].substring(0, 1) != '+')
        break;
    if (options[arguments[i].substring(1)] == undefined)
        die("invalid command line option \"" + arguments[i] + "\"");
    options[arguments[i].substring(1)] =
        (arguments[i].substring(0, 1) == "-" ? true : false);
}
if (arguments[i] == undefined) {
    usage()
}

var file = new File(arguments[i]);
file.open("read");
var script = file.readAll();
file.close();

if (!jslint(script, { passfail: true })) {
    var e = jslint.errors[0];
    print('jslint: line ' + (e.line + 1) + ' character ' + (e.character + 1) + ': ' + e.reason);
    print((e.evidence || ''). replace(/^\s*(\S*(\s+\S+)*)\s*$/, "$1"));
    quit();
}

