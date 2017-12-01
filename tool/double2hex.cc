#include <iostream>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <cstring>

typedef std::bitset<64> real_bit_set;

union value {
  double real;
  std::uint64_t raw;
};

int main( int argc , char* argv[] ) {
  if(argc != 2 && argc != 3) {
    std::cerr<<"usage: <real>\n";
    return -1;
  }

  double v;
  {
    char* pend = NULL;
    v = std::strtod(argv[1],&pend);
    if( errno || *pend ) {
      std::cerr<<"cannot convert "<<argv[1]<<" to number/real/double!";
      return -1;
    }
  }

  value val; val.real = v;

  if(argc == 3 && strcmp(argv[2],"bin") == 0) {
    real_bit_set b(val.raw);
    std::cout<<b<<std::endl;
  } else {
    std::cout << std::hex << val.raw << std::endl;
    std::cout << "High: " << (val.raw >> 32) << std::endl;
    std::cout << "Low : " << (val.raw & 0xffffffff) << std::endl;
  }
  return 0;
}

