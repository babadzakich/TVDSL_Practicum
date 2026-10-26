#include "Token.h"

#include <cctype>
#include <cstdio>

namespace funny {

namespace {

std::string bad_chars_message(std::string_view text) {
  return (text.size() == 1 ? "недопустимый символ " : "недопустимые символы ") + quote_lexeme(text);
}

const char* punctuation(TokenKind kind) {
  switch (kind) {
    case TokenKind::EQ: return "==";
    case TokenKind::NE: return "!=";
    case TokenKind::LE: return "<=";
    case TokenKind::GE: return ">=";
    case TokenKind::ARROW: return "->";
    case TokenKind::FAT_ARROW: return "=>";
    case TokenKind::LT: return "<";
    case TokenKind::GT: return ">";
    case TokenKind::ASSIGN: return "=";
    case TokenKind::PLUS: return "+";
    case TokenKind::MINUS: return "-";
    case TokenKind::STAR: return "*";
    case TokenKind::SLASH: return "/";
    case TokenKind::LPAREN: return "(";
    case TokenKind::RPAREN: return ")";
    case TokenKind::LBRACKET: return "[";
    case TokenKind::RBRACKET: return "]";
    case TokenKind::LBRACE: return "{";
    case TokenKind::RBRACE: return "}";
    case TokenKind::COMMA: return ",";
    case TokenKind::SEMICOLON: return ";";
    case TokenKind::COLON: return ":";
    case TokenKind::PIPE: return "|";
    default: return nullptr;
  }
}

// Текущая позиция при проходе по тексту.
class Cursor {
 public:
  explicit Cursor(std::string_view src) : src_(src) {}

  std::size_t offset() const { return pos_.offset; }
  const Pos& pos() const { return pos_; }

  void advance(std::size_t n) {
    for (std::size_t end = pos_.offset + n; pos_.offset < end; ++pos_.offset) {
      if (src_[pos_.offset] == '\n') {
        ++pos_.line;
        pos_.col = 1;
      } else {
        ++pos_.col;
      }
    }
  }

 private:
  std::string_view src_;
  Pos pos_;
};

}  // namespace

LexResult lex(std::string_view source) {
  LexResult r;
  Cursor cur(source);
  while (cur.offset() < source.size()) {
    const std::size_t at = cur.offset();
    const funny_lexer::Match m = funny_lexer::next_token(source, at);
    const std::string_view text = source.substr(at, m.length);

    if (m.kind == TokenKind::COMMENT) {
      const std::size_t end = at + m.length;
      cur.advance(m.length);
      if (end < source.size() && source[end] != '\n') {
        // Комментарий оборвался на не-ASCII байте: одно сообщение, остаток строки — тоже комментарий.
        r.diagnostics.push_back({cur.pos(), "недопустимые символы в комментарии (разрешён только ASCII)"});
        const std::size_t nl = source.find('\n', end);
        cur.advance((nl == std::string_view::npos ? source.size() : nl) - end);
      }
      continue;
    }
    if (funny_lexer::is_skip(m.kind)) {
      cur.advance(m.length);
      continue;
    }
    if (m.kind == TokenKind::ERROR) {
      Token* prev = r.tokens.empty() ? nullptr : &r.tokens.back();
      if (prev != nullptr && prev->kind == TokenKind::ERROR && prev->pos.offset + prev->text.size() == at) {
        prev->text = source.substr(prev->pos.offset, prev->text.size() + m.length);
        r.diagnostics.back().message = bad_chars_message(prev->text);
      } else {
        r.tokens.push_back({TokenKind::ERROR, text, cur.pos()});
        r.diagnostics.push_back({cur.pos(), bad_chars_message(text)});
      }
      cur.advance(m.length);
      continue;
    }
    TokenKind kind = m.kind;
    if (kind == TokenKind::BAD_INT) {
      r.diagnostics.push_back(
          {cur.pos(), "некорректное число " + quote_lexeme(text) + ": ведущие нули не допускаются"});
      kind = TokenKind::INT;
    }
    r.tokens.push_back({kind, text, cur.pos()});
    cur.advance(m.length);
  }
  r.tokens.push_back({kEof, source.substr(source.size()), cur.pos()});
  return r;
}

std::string describe_kind(TokenKind kind) {
  if (kind == kEof) {
    return "конец файла";
  }
  if (kind == TokenKind::IDENT) {
    return "идентификатор";
  }
  if (kind == TokenKind::INT || kind == TokenKind::BAD_INT) {
    return "число";
  }
  if (const char* p = punctuation(kind)) {
    return quote_lexeme(p);
  }
  std::string name = funny_lexer::token_name(kind);
  if (name.starts_with("KW_")) {
    name.erase(0, 3);
    for (char& c : name) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return quote_lexeme(name);
  }
  return name;
}

std::string quote_lexeme(std::string_view s) {
  std::string out = "'";
  for (char c : s) {
    const auto b = static_cast<unsigned char>(c);
    if (b < 0x20 || b >= 0x7F) {  // вне печатного ASCII
      char buf[5];
      std::snprintf(buf, sizeof buf, "\\x%02X", b);
      out += buf;
    } else {
      out += c;
    }
  }
  out += '\'';
  return out;
}

}  // namespace funny
