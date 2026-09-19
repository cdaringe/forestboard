#pragma once
constexpr int PNUM_NOT_DEFINED = -1;
class SPIClass {
public:
  template <class... T> SPIClass(T...) {}
};
