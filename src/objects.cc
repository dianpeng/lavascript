#include "objects.h"

namespace lavascript {

Entry* Map::FindEntry( const Value& key , std::uint32_t fullhash ,
                       const Value& value , Option opt ) {
  int main_position = fullhash & (capacity()-1);

  Entry* main = entry()[main_position];

  if(!main->use) return opt == FIND ? NULL : main;

  // Okay the main entry is been used or at least it is a on chain of the
  // collision list. So we need to chase down the link to see whats happening
  Entry* cur = main;

  do {
    if(!cur->del) {
      // The current entry is not deleted, so we can try check wether it is a
      // matched key or not
      if(cur->hash == fullhash && KeyEqual(cur->key,key)) {
        // Find an entry that is exactly the same key as specified
        return opt == INSERT ? NULL : cur;
      }
    }
    if(cur->more)
      cur = entry()[cur->next];
    else
      break;
  } while(true);

  if(opt == FIND) return NULL;

  // Linear probing
  Entry* new_slot = NULL;
  std::uint32_t h = fullhash;
  while( (new_slot = entry()[ ++h &(capacity()-1)])->use )
    ;

  return new_slot;
}

} // namespace lavascript
