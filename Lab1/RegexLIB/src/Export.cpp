#include "Export.h"

#include <cstdio>
#include <sstream>

namespace funnylex {

namespace {

std::string hex_byte(unsigned char c) {
  char buf[5];
  std::snprintf(buf, sizeof(buf), "\\x%02X", c);
  return buf;
}

std::string json_string(std::string_view s) {
  std::string out = "\"";
  for (const char ch : s) {
    const auto c = static_cast<unsigned char>(ch);
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\r':
        out += "\\r";
        break;
      default:
        if (c < 0x20 || c >= 0x7F) {
          char buf[7];
          std::snprintf(buf, sizeof(buf), "\\u%04X", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out + "\"";
}

int count_nfa_edges(const Nfa& nfa) {
  int edges = 0;
  for (const NfaState& s : nfa.states) {
    edges += static_cast<int>(s.edges.size());
  }
  return edges;
}

}  // namespace

std::string escape_string(std::string_view s) {
  std::string out;
  for (const char ch : s) {
    const auto c = static_cast<unsigned char>(ch);
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\r':
        out += "\\r";
        break;
      default:
        if (c < 0x20 || c >= 0x7F) {
          out += hex_byte(c);
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

std::string export_json(const BuildResult& build) {
  const LexerTable& t = build.table;
  std::ostringstream out;
  out << "{\n";
  out << "  \"alphabet\": \"ASCII (0..127); bytes 128..255 go to trap\",\n";
  out << "  \"start\": " << t.start << ",\n";
  out << "  \"trap\": " << t.trap << ",\n";
  out << "  \"num_states\": " << t.num_states() << ",\n";
  out << "  \"num_classes\": " << t.num_classes << ",\n";
  out << "  \"stats\": {\"nfa_states\": " << build.nfa.size() << ", \"nfa_edges\": " << count_nfa_edges(build.nfa)
      << ", \"dfa_states\": " << build.dfa.size() << ", \"min_dfa_states\": " << build.min_dfa.size() << "},\n";

  out << "  \"tokens\": [\n";
  for (size_t i = 0; i < build.spec.rules.size(); ++i) {
    const TokenRule& r = build.spec.rules[i];
    out << "    {\"id\": " << i << ", \"name\": " << json_string(r.name) << ", \"regex\": " << json_string(r.pattern)
        << ", \"skip\": " << (r.skip ? "true" : "false") << ", \"error\": " << (r.error ? "true" : "false") << "}"
        << (i + 1 < build.spec.rules.size() ? "," : "") << "\n";
  }
  out << "  ],\n";

  out << "  \"byte_to_class\": [";
  for (int b = 0; b < 256; ++b) {
    out << (b % 32 == 0 ? "\n    " : " ") << t.byte_class[b] << (b < 255 ? "," : "");
  }
  out << "\n  ],\n";

  out << "  \"states\": [\n";
  for (int s = 0; s < t.num_states(); ++s) {
    out << "    {\"id\": " << s << ", \"accept\": ";
    if (t.accept[s] >= 0) {
      out << json_string(t.token_names[t.accept[s]]);
    } else {
      out << "null";
    }
    out << ", \"next\": [";
    for (int c = 0; c < t.num_classes; ++c) {
      out << (c ? ", " : "") << t.next[s][c];
    }
    out << "]}" << (s + 1 < t.num_states() ? "," : "") << "\n";
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

std::string export_cpp_header(const BuildResult& build, const std::string& ns) {
  const LexerTable& t = build.table;
  const int num_tokens = static_cast<int>(t.token_names.size());
  const char* class_type = t.num_classes <= 256 ? "std::uint8_t" : "std::uint16_t";
  const char* state_type = t.num_states() <= 256 ? "std::uint8_t" : "std::uint16_t";

  std::ostringstream out;
  out << "// Сгенерировано funnylex (HW1) — не редактировать вручную.\n"
      << "// Минимальный ДКА лексера Funny: " << t.num_states() << " состояний (ловушка = " << t.trap
      << ", старт = " << t.start << "), " << t.num_classes << " классов байтов.\n"
      << "// Переход: kNext[state][kByteClass[byte]]. Байты 128..255 всегда ведут в ловушку.\n"
      << "#pragma once\n\n"
      << "#include <cstddef>\n#include <cstdint>\n#include <string_view>\n\n"
      << "namespace " << ns << " {\n\n";

  out << "enum class TokenKind : int {\n  ERROR = -1,  // символ, попавший в ловушку\n";
  for (int i = 0; i < num_tokens; ++i) {
    out << "  " << t.token_names[i] << " = " << i << ",\n";
  }
  out << "};\n\n";

  out << "inline constexpr int kNumTokens = " << num_tokens << ";\n"
      << "inline constexpr int kNumStates = " << t.num_states() << ";\n"
      << "inline constexpr int kNumClasses = " << t.num_classes << ";\n"
      << "inline constexpr int kStartState = " << t.start << ";\n"
      << "inline constexpr int kTrapState = " << t.trap << ";\n\n";

  out << "inline constexpr " << class_type << " kByteClass[256] = {";
  for (int b = 0; b < 256; ++b) {
    out << (b % 16 == 0 ? "\n    " : " ") << t.byte_class[b] << ",";
  }
  out << "\n};\n\n";

  out << "inline constexpr " << state_type << " kNext[kNumStates][kNumClasses] = {\n";
  for (int s = 0; s < t.num_states(); ++s) {
    out << "    {";
    for (int c = 0; c < t.num_classes; ++c) {
      out << (c ? ", " : "") << t.next[s][c];
    }
    out << "},  // " << s;
    if (t.accept[s] >= 0) {
      out << " " << t.token_names[t.accept[s]];
    }
    out << "\n";
  }
  out << "};\n\n";

  out << "// Токен, принимаемый состоянием, или -1.\n"
      << "inline constexpr std::int16_t kAccept[kNumStates] = {";
  for (int s = 0; s < t.num_states(); ++s) {
    out << (s % 16 == 0 ? "\n    " : " ") << t.accept[s] << ",";
  }
  out << "\n};\n\n";

  auto bool_array = [&](const char* name, const std::vector<bool>& values) {
    out << "inline constexpr bool " << name << "[kNumTokens] = {";
    for (int i = 0; i < num_tokens; ++i) {
      out << (i % 16 == 0 ? "\n    " : " ") << (values[i] ? "true" : "false") << ",";
    }
    out << "\n};\n\n";
  };
  bool_array("kSkip", t.skip);
  bool_array("kIsError", t.is_error);

  out << "inline constexpr const char* kTokenNames[kNumTokens] = {";
  for (int i = 0; i < num_tokens; ++i) {
    out << (i % 4 == 0 ? "\n    " : " ") << "\"" << t.token_names[i] << "\",";
  }
  out << "\n};\n\n";

  out << R"(constexpr const char* token_name(TokenKind kind) {
  return kind == TokenKind::ERROR ? "ERROR" : kTokenNames[static_cast<int>(kind)];
}

constexpr bool is_skip(TokenKind kind) { return kind != TokenKind::ERROR && kSkip[static_cast<int>(kind)]; }

constexpr bool is_error(TokenKind kind) {
  return kind == TokenKind::ERROR || kIsError[static_cast<int>(kind)];
}

struct Match {
  TokenKind kind;
  std::size_t length;  // всегда > 0
};

// Самый длинный токен, начинающийся с pos (требуется pos < input.size()).
// Если ни один префикс не принимается — ERROR длиной в один символ
// (не-ASCII символ UTF-8 забирается целиком).
constexpr Match next_token(std::string_view input, std::size_t pos) {
  int state = kStartState;
  int last_kind = -1;
  std::size_t last_end = pos;
  for (std::size_t i = pos; i < input.size(); ++i) {
    state = kNext[state][kByteClass[static_cast<unsigned char>(input[i])]];
    if (state == kTrapState) {
      break;
    }
    if (kAccept[state] >= 0) {
      last_kind = kAccept[state];
      last_end = i + 1;
    }
  }
  if (last_kind >= 0) {
    return {static_cast<TokenKind>(last_kind), last_end - pos};
  }
  std::size_t end = pos + 1;
  if (static_cast<unsigned char>(input[pos]) >= 0xC0) {
    while (end < input.size() && (static_cast<unsigned char>(input[end]) & 0xC0) == 0x80) {
      ++end;
    }
  }
  return {TokenKind::ERROR, end - pos};
}

)";
  out << "}  // namespace " << ns << "\n";
  return out.str();
}

}  // namespace funnylex
