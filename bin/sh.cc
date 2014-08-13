// Shell.

#include "ref.hh"
#include "libutil.h"
#include "xsys.h"

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <stdexcept>
#include <string>
#include <vector>

using std::move;
using std::pair;
using std::string;
using std::swap;
using std::vector;

bool interactive = true;

int fork1(void);  // Fork but panics on failure.

//////////////////////////////////////////////////////////////////
// Environment
//

vector<pair<string, string> > vars;

void
setvar(const string &name, const string &val)
{
  for (auto &p : vars) {
    if (p.first == name) {
      p.second = val;
      return;
    }
  }
  vars.push_back({name, val});
}

string
getvar(const string &name)
{
  for (auto &p : vars)
    if (p.first == name)
      return p.second;
  return string();
}

//////////////////////////////////////////////////////////////////
// Commands
//

// XXX xv6 now supports O_CLOEXEC.  Use it.
class savefd
{
  static vector<int> cloexec;
  int fd_, saved_;

public:
  savefd(int fd) : fd_(fd), saved_(dup(fd))
  {
    cloexec.push_back(saved_);
  }

  ~savefd()
  {
    close(fd_);
    dup(saved_);
    close(saved_);
    assert(cloexec.back() == saved_);
    cloexec.pop_back();
  }

  static void preexec()
  {
    for (int fd : cloexec)
      close(fd);
  }
};
vector<int> savefd::cloexec;

class cmd : public referenced
{
public:
  virtual ~cmd() { }
  virtual int run() = 0;
};

class expr : public referenced
{
public:
  virtual ~expr() { }
  virtual void eval(vector<string> *out) = 0;

  string eval_join()
  {
    vector<string> words;
    eval(&words);
    string res;
    bool first = true;
    for (auto &w : words) {
      if (!first)
        res += ' ';
      first = false;
      res += w;
    }
    return res;
  }
};

class cmd_exec : public cmd
{
public:
  vector<sref<expr> > argv_;
  vector<pair<string, sref<expr> > > vars_;

  int run() override
  {
    if (argv_.empty()) {
      // Assign shell variables
      for (auto &p : vars_) {
        string val = p.second->eval_join();
        setvar(p.first, val);
      }
      return 0;
    }

    vector<string> argv;
    for (auto &expr : argv_)
      expr->eval(&argv);

    // Handle built-ins
    if (argv[0] == "cd") {
      if (argv.size() != 2) {
        fprintf(stderr, "cd: wrong number of arguments");
        return 1;
      }
      if (chdir(argv[1].c_str()) < 0) {
        fprintf(stderr, "cd: failed\n");
        return 1;
      }
      return 0;
    }
    if (argv[0] == "exit") {
      exit(0);
    }

    // Not a built-in
    vector<const char *> argstrs;
    for (auto &arg : argv)
      argstrs.push_back(arg.c_str());
    argstrs.push_back(nullptr);

    // XXX posix_spawn
    int child;
    if ((child = fork1()) == 0) {
      savefd::preexec();
      execv(argstrs[0], const_cast<char * const *>(argstrs.data()));
      edie("exec %s failed", argstrs[0]);
    }
    int status;
    if (waitpid(child, &status, 0) < 0)
      edie("wait failed");
    return status;
  }
};

class cmd_redir : public cmd
{
  int fd_, mode_;
  sref<expr> file_;
  sref<cmd> left_;

public:
  cmd_redir(int fd, int mode, const sref<expr> &file, const sref<cmd> &left)
    : fd_(fd), mode_(mode), file_(file), left_(left) { }

  int run() override
  {
    vector<string> files;
    file_->eval(&files);
    if (files.size() != 1) {
      fprintf(stderr, "redirection requires exactly one file name");
      return 1;
    }

    savefd saved(fd_);
    close(fd_);
    if (open(files[0].c_str(), mode_, 0666) < 0) {
      fprintf(stderr, "sh: open %s failed\n", files[0].c_str());
      return 1;
    }
    return left_->run();
  }
};

class cmd_pipe : public cmd
{
  sref<cmd> left_, right_;

public:
  cmd_pipe(const sref<cmd> &left, const sref<cmd> &right)
    : left_(left), right_(right) { }

  int run() override
  {
    int p[2];
    if (pipe(p) < 0)
      edie("pipe");

    int child;
    if ((child = fork1()) == 0) {
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      exit(left_->run());
    }

    savefd saved(0);
    close(0);
    dup(p[0]);
    close(p[0]);
    close(p[1]);
    int res = right_->run();
    if (waitpid(child, NULL, 0) < 0)
      edie("wait failed");
    return res;
  }
};

class cmd_back : public cmd
{
  sref<cmd> cmd_;

public:
  cmd_back(const sref<cmd> &cmd)
    : cmd_(cmd) { }

  int run() override
  {
    if (fork1() == 0)
      exit(cmd_->run());
    return 0;
  }
};

class cmd_for : public cmd
{
  string var_;
  vector<sref<expr> > lst_;
  sref<cmd> body_;

public:
  cmd_for(string var, vector<sref<expr> > &&lst, sref<cmd> body)
    : var_(move(var)), lst_(move(lst)), body_(move(body)) { }

  int run() override
  {
    vector<string> lst;
    for (auto &e : lst_)
      e->eval(&lst);
    int last = 0;
    for (auto &val : lst) {
      setvar(var_, val);
      last = body_->run();
    }
    return last;
  }
};

class cmd_list : public cmd
{
  sref<cmd> left_, right_;
  char op_;

public:
  cmd_list(const sref<cmd> &left, const sref<cmd> &right, char op = ';')
    : left_(left), right_(right), op_(op) { }

  int run() override
  {
    int l = left_->run();
    if ((op_ == 'A' && l != 0) || (op_ == 'O' && l == 0))
      return l;
    return right_->run();
  }
};

class cmd_error : public cmd
{
  const string error_;

public:
  cmd_error(const string &error) : error_(error) { }

  int run() override
  {
    fprintf(stderr, "sh: %s\n", error_.c_str());
    return 1;
  }
};

class expr_word : public expr
{
  const string val_;

public:
  expr_word(const string val)
    : val_(val) { }

  void eval(vector<string> *out) override
  {
    out->push_back(val_);
  }
};

class expr_var : public expr
{
  const string var_;

public:
  expr_var(const string var)
    : var_(var) { }

  void eval(vector<string> *out) override
  {
    out->push_back(getvar(var_));
  }
};

class expr_brace : public expr
{
  sref<expr> body_;

public:
  expr_brace(sref<expr> body)
    : body_(body) { }

  void eval(vector<string> *out) override
  {
    vector<string> body;
    char *send, *eend, *xend;
    long start, end, step;

    body_->eval(&body);
    if (body.size() != 1)
      goto bad;

    // Shell is insane:
    //   X=1.; echo {$X.3} => 1 2 3
    //   X="{1"; echo $X..3} => syntax error
    //   echo {"1.".3} => 1 2 3
    // In other words, we have to parse here.
    start = strtol(body[0].c_str(), &send, 10);
    if (send == body[0].c_str() || strncmp(send, "..", 2))
      goto bad;
    end = strtol(send + 2, &eend, 10);
    if (eend == send + 2)
      goto bad;
    if (*eend == 0) {
      step = 1;
    } else if (strncmp(eend, "..", 2) == 0) {
      step = strtol(eend + 2, &xend, 10);
      if (*xend)
        goto bad;
      if (step < 0) {
        swap(start, end);
        step = -step;
      } else if (step == 0) {
        goto bad;
      }
    } else {
      goto bad;
    }

    if (start > end)
      step = -step;

    for (long val = start; (step > 0) ? val <= end : end <= val; val += step) {
      char buf[64];
      snprintf(buf, sizeof buf, "%ld", val);
      out->push_back(buf);
    }
    return;

  bad:
    // XXX No way to return an error from here
    fprintf(stderr, "sh: syntax error: bad brace expansion\n");
  }
};

class expr_concat : public expr
{
  sref<expr> left_, right_;

public:
  expr_concat(const sref<expr> &left, const sref<expr> &right)
    : left_(left), right_(right) { }

  void eval(vector<string> *out) override
  {
    // bash/zsh have some crazy and inconsistent rules about how to
    // concatenate list-like expressions.  We always use Cartesian
    // product.
    vector<string> left, right;
    left_->eval(&left);
    right_->eval(&right);
    for (auto &r : right)
      for (auto &l : left)
        out->push_back(l + r);
  }
};

//////////////////////////////////////////////////////////////////
// Parser
//

class syntax_error : public std::runtime_error
{
public:
  explicit syntax_error(const char* what_arg)
    : runtime_error(what_arg) { }
};

class syntax_incomplete : public std::exception
{
};

struct tok
{
  char type;
  string word;

  string describe() const
  {
    if (word.empty())
      return string() + type;
    return word;
  }
};

void
lex(const string &buf, bool eof, vector<tok> *toks)
{
  static constexpr const char *whitespace = " \t\r\n\v";
  static constexpr const char *symbols = "<>|&;()";

  string::const_iterator pos(buf.begin()), end(buf.end());
  enum {
    NORMAL, WORD, SINGLE, DOUBLE
  } mode = NORMAL;
  string word;

  while (pos < end) {
    if (mode == NORMAL) {
      while (pos < end && strchr(whitespace, *pos)) {
        if (*pos == '\n')
          // Semicolon injection
          toks->push_back(tok{';'});
        ++pos;
      }
    }
    if (pos == end)
      break;

    switch (mode) {
    case NORMAL:
      if (strchr(symbols, *pos)) {
        char next = ((pos + 1) < end) ? *(pos + 1) : 0;
        if (*pos == '>' && next == '>') {
          toks->push_back(tok{'+', ">>"});
          pos += 2;
        } else if (*pos == '&' && next == '&') {
          toks->push_back(tok{'A', "&&"});
          pos += 2;
        } else if (*pos == '|' && next == '|') {
          toks->push_back(tok{'O', "||"});
          pos += 2;
        } else
          toks->push_back(tok{*(pos++)});
      } else if (*pos == '#') {
        while (pos < end && *pos != '\n' && *pos != '\r')
          ++pos;
      } else {
        word.clear();
        mode = WORD;
      }
      break;

    case WORD:
      if (strchr(whitespace, *pos) || strchr(symbols, *pos) || *pos == '#') {
        toks->push_back(tok{'a', move(word)});
        toks->push_back(tok{' '});
        word.clear();
        mode = NORMAL;
        continue;
      } else if (*pos == '\'') {
        mode = SINGLE;
      } else if (*pos == '\"') {
        mode = DOUBLE;
      } else if (*pos == '\\' && (pos+1) < end) {
        ++pos;
        if (*pos != '\n')
          word += *(pos++);
        else if (pos+1 == end && !eof)
          // Escaped newline.  Get more input.
          throw syntax_incomplete();
      } else if (*pos == '$' && (pos+1) < end) {
        goto do_var;
      } else if (strchr("{}", *pos)) {
        toks->push_back(tok{'a', move(word)});
        toks->push_back(tok{*pos});
        word.clear();
      } else {
        word += *pos;
        if (*pos == '=' && word.size() > 1) {
          toks->push_back(tok{'=', move(word)});
          word.clear();
        }
      }
      ++pos;
      break;

    case SINGLE:
      if (*pos == '\'')
        mode = WORD;
      else
        word += *pos;
      ++pos;
      break;

    case DOUBLE:
      if (*pos == '\"')
        mode = WORD;
      else if (*pos == '\\' && *(pos+1) == '\n')
        ++pos;
      else if (*pos == '\\' && strchr("$`\"\\", *(pos+1)))
        word += *++pos;
      else if (*pos == '$' && (pos+1) < end)
        goto do_var;
      else
        word += *pos;
      ++pos;
      break;

    do_var:
      ++pos;
      toks->push_back(tok{'a', move(word)});
      word.clear();
      if (*pos == '{') {
        ++pos;
        while (pos < end && *pos != '}')
          word += *(pos++);
        if (*pos != '}') {
          if (eof)
            throw syntax_error("unterminated substitution");
          throw syntax_incomplete();
        }
        ++pos;
      } else {
        while (pos < end && (isalnum(*pos) || *pos == '_'))
          word += *(pos++);
      }
      toks->push_back(tok{'$', move(word)});
      word.clear();
      break;
    }
  }

  switch (mode) {
  case WORD:
    assert(pos == end);
    toks->push_back(tok{'a', move(word)});
    toks->push_back(tok{' '});
  case NORMAL:
    break;
  case SINGLE:
  case DOUBLE:
    if (eof)
      throw syntax_error("unterminated quoted string");
    throw syntax_incomplete();
  }
}

class parser
{
  vector<tok> toks;
  vector<tok>::const_iterator cur;
  bool eof;

  char tryget(const char *accept, string *word_out = nullptr)
  {
    if (cur == toks.end() || !strchr(accept, cur->type))
      return 0;
    char type = cur->type;
    if (word_out)
      *word_out = cur->word;
    ++cur;
    return type;
  }

  char require(const char *accept, const char *error, string *word_out = nullptr)
  {
    if (!eof && cur == toks.end())
      throw syntax_incomplete();
    char type = tryget(accept, word_out);
    if (!type)
      throw syntax_error(error);
    return type;
  }

  bool trygetword(const char *word)
  {
    if (cur == toks.end() || cur->type != 'a' || cur->word != word ||
        cur + 1 == toks.end() || (cur+1)->type != ' ')
      return false;
    cur += 2;
    return true;
  }

  void requireword(const char *word, const char *error)
  {
    if (!eof && cur == toks.end())
      throw syntax_incomplete();
    if (!trygetword(word))
      throw syntax_error(error);
  }

  sref<cmd> pexec()
  {
    sref<cmd> res;
    if (tryget("(")) {
      res = plist();
      require(")", "missing ')'");
      res = predir(res);
      return res;
    } else if ((res = pfor())) {
      res = predir(res);
      return res;
    } else if (trygetword("done")) {
      cur -= 2;                 // Unget
      return sref<cmd>::transfer(new cmd_exec());
    }

    cmd_exec *ex = new cmd_exec();
    res = sref<cmd>::transfer(ex);
    bool may_assign = true;
    while (true) {
      res = predir(res);

      string var;
      if (may_assign && tryget("=", &var)) {
        sref<expr> val = pexpr();
        assert(val);
        var.pop_back();         // Remove '=' on the end
        ex->vars_.emplace_back(var, val);
        continue;
      } else {
        may_assign = false;
      }

      sref<expr> word = pexpr();
      if (!word)
        break;
      ex->argv_.push_back(move(word));
    }
    return res;
  }

  sref<cmd> pfor()
  {
    if (!trygetword("for"))
      return sref<cmd>();
    string var;
    require("a", "expected for loop variable", &var);
    require(" ", "bad for loop variable");
    requireword("in", "expected 'in'");
    vector<sref<expr> > lst;
    while (true) {
      sref<expr> elt;
      if (!(elt = pexpr()))
        break;
      lst.push_back(move(elt));
    }
    require(";", "expected semicolon");
    requireword("do", "expected 'do'");
    auto body = plist();
    requireword("done", "expected 'done'");
    return sref<cmd>::transfer(new cmd_for{move(var), move(lst), move(body)});
  }

  sref<cmd> predir(sref<cmd> res)
  {
    char tok;
    while ((tok = tryget("<>+"))) {
      sref<expr> file = pexpr();
      if (!file)
        throw syntax_error("missing file for redirection");
      switch (tok) {
      case '<':
        res = sref<cmd>::transfer(new cmd_redir{0, O_RDONLY, file, res});
        break;
      case '>':
        res = sref<cmd>::transfer(
          new cmd_redir{1, O_WRONLY|O_CREAT|O_TRUNC, file, res});
        break;
      case '+':                 // >>
        res = sref<cmd>::transfer(
          new cmd_redir{1, O_WRONLY|O_CREAT|O_APPEND, file, res});
        break;
      }
    }
    return res;
  }

  sref<cmd> ppipe()
  {
    auto res = pexec();
    if (tryget("|"))
      res = sref<cmd>::transfer(new cmd_pipe{res, ppipe()});
    return res;
  }

  // and_or <- pipeline ((AND_IF | OR_IF) linebreak pipeline)* [left]
  sref<cmd> pand_or()
  {
    auto res = ppipe();
    while (char op = tryget("AO"))
      res = sref<cmd>::transfer(new cmd_list{res, ppipe(), op});
    return res;
  }

  // list <- and_or (separator_op and_or)*
  sref<cmd> plist()
  {
    auto l = pand_or();
    char sep = tryget(";&");
    if (!sep)
      return l;
    if (sep == '&')
      l = sref<cmd>::transfer(new cmd_back{l});
    return sref<cmd>::transfer(new cmd_list{l, plist()});
  }

  sref<expr> pexpr(const char *term = " ", char *last_out = nullptr)
  {
    // Note that word sequences always begin with an 'a' or '=' token,
    // even if that token is empty.
    if (cur == toks.end() || !strchr("a=", cur->type))
      return sref<expr>();
    string part;
    sref<expr> res;
    char last;
    while (!(last = tryget(term))) {
      string buf;
      bool any = false;
      while (tryget("a=", &part)) {
        buf += part;
        any = true;
      }
      sref<expr> word;
      if (any) {
        word = sref<expr>::transfer(new expr_word{move(buf)});
      } else if (tryget("$", &part)) {
        word = sref<expr>::transfer(new expr_var{move(part)});
      } else if (tryget("{")) {
        char last;
        word = sref<expr>::transfer(new expr_brace{pexpr(" }", &last)});
        if (last != '}')
          throw syntax_error("expected '}'");
      } else if (tryget("}")) {
        throw syntax_error("unexpected '}'");
      } else {
        die("unexpected token type in expression context");
      }
      if (!res)
        res = move(word);
      else
        res = sref<expr>::transfer(new expr_concat{res, move(word)});
    }
    if (last_out)
      *last_out = last;
    return res;
  }

public:
  sref<cmd> res;
  bool incomplete;

  parser(const string &buf, bool eof) : eof(eof), incomplete(false)
  {
    try {
      lex(buf, eof, &toks);
      cur = toks.begin();
      res = plist();
      if (cur != toks.end())
        res = sref<cmd>::transfer(
          new cmd_error{string("unexpected token: ") + cur->describe()});
    } catch (syntax_error &error) {
      res = sref<cmd>::transfer(
        new cmd_error{string("syntax error: ") + error.what()});
    } catch (syntax_incomplete &x) {
      assert(!eof);
      incomplete = true;
    }
    vector<tok>().swap(toks);
  }
};

//////////////////////////////////////////////////////////////////
// Main
//

string
readline(bool continuation)
{
  if (interactive)
    fprintf(stderr, continuation ? "> " : "$ ");
  string line;
  while (true) {
    char c;
    size_t n = read(0, &c, 1);
    if (n < 1)
      break;
    line += c;
    if (c == '\n' || c == '\r')
      break;
  }
  return line;
}

int
main(int argc, char** argv)
{
  string buf;

  // If args, concatenate them parse as a command.
  if (argc > 1 && strcmp(argv[1], "-c") == 0) {
    for (int i = 2; i < argc; i++) {
      if (i > 2)
        buf.push_back(' ');
      buf.append(argv[i]);
    }

    parser p(buf, true);
    exit(p.res->run());
  } else if (argc > 1) {
    // Shell script
    interactive = false;
    close(0);
    if (open(argv[1], O_RDONLY) < 0) {
      fprintf(stderr, "cannot open %s\n", argv[1]);
      return -1;
    }
  }

  // Read and run input commands.
  int last = 0;
  while (true) {
    string line = readline(!buf.empty());
    if (line.empty()) {
      if (!buf.empty()) {
        // Give EOF error
        parser p(buf, true);
        exit(p.res->run());
      }
      break;
    }
    buf += line;

    parser p(buf, false);
    if (p.incomplete)
      continue;
    buf.clear();
    last = p.res->run();
  }
  return last;
}

int
fork1(void)
{
  int pid;
  
  pid = fork();
  if (pid == -1)
    die("fork");
  return pid;
}
