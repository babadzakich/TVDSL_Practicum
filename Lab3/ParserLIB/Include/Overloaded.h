// Набор лямбд для std::visit.

#ifndef FUNNYPARSE_OVERLOADED_H
#define FUNNYPARSE_OVERLOADED_H

namespace funny {

template <class... Fs>
struct Overloaded : Fs... {
  using Fs::operator()...;
};

}  // namespace funny

#endif  // FUNNYPARSE_OVERLOADED_H
