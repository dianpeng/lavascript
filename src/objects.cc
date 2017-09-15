#include "objects.h"
#include "gc.h"

namespace lavascript {

/* ---------------------------------------------------------------
 * String
 * --------------------------------------------------------------*/
Handle<String> String::New( GC* gc ) {
  return Handle<String>(gc->NewString());
}

Handle<String> String::New( GC* gc , const char* str , std::size_t length ) {
  return Handle<String>(gc->NewString(str,length));
}

/* ---------------------------------------------------------------
 * List
 * --------------------------------------------------------------*/
Handle<List> List::New( GC* gc ) {
  Handle<Slice> slice(gc->NewSlice());
  return Handle<List>(gc->New<List>(slice));
}

Handle<List> List::New( GC* gc , size_t capacity ) {
  Handle<Slice> slice(gc->NewSlice(capacity));
  return Handle<List>(gc->New<List>(slice));
}

Handle<List> List::New( GC* gc , const Handle<Slice>& slice ) {
  return Handle<List>(gc->New<List>(slice));
}

/* ---------------------------------------------------------------
 * Slice
 * -------------------------------------------------------------*/
Handle<Slice> Slice::Extend( GC* gc, const Handle<Slice>& old , size_t new_cap ) {
  Handle<Slice> new_slice(gc->NewSlice(new_cap));
  memcpy(new_slice->data(),old->data(),sizeof(Value)*old->capacity());
  return new_slice;
}

Handle<Slice> Slice::New( GC* gc ) {
  return Handle<Slice>(gc->NewSlice());
}

Handle<Slice> Slice::New( GC* gc , std::size_t cap ) {
  return Handle<Slice>(gc->NewSlice(cap));
}

/* ---------------------------------------------------------------
 * Object
 * --------------------------------------------------------------*/
Handle<Object> Object::New( GC* gc ) {
  return Handle<Object>(gc->New<Object>(gc->NewMap()));
}

Handle<Object> Object::New( GC* gc , std::size_t capacity ) {
  return Handle<Object>(gc->New<Object>(gc->NewMap(capacity)));
}

Handle<Object> Object::New( GC* gc , const Handle<Map>& map ) {
  return Handle<Object>(gc->New<Object>(map));
}

/* ---------------------------------------------------------------
 * Map
 * --------------------------------------------------------------*/
Handle<Map> Map::New( GC* gc ) {
  return Handle<Map>(gc->NewMap());
}

Handle<Map> Map::New( GC* gc , std::size_t capacity ) {
  return Handle<Map>(gc->NewMap(capacity));
}

Handle<Map> Map::Rehash( GC* gc , const Handle<Map>& old_map ) {
  std::size_t new_cap = old_map->capacity() * 2;

  Handle<Map> new_map(gc->NewMap(new_cap));

  const std::size_t len = old_map->size();

  for( std::size_t i = 0 ; i < len ; ++i ) {
    const Entry* e = old_map->data()+i;
    if(e->active()) {
      Entry* new_entry = new_map->FindEntry(e->key.GetString(),
                                            e->hash,
                                            INSERT);
#ifdef LAVASCRIPT_CHECK_OBJECTS
      lava_verify(new_entry);
#endif // LAVASCRIPT_CHECK_OBJECTS

      *new_entry = *e;
    }
  }

  return new_map;
}

} // namespace lavascript
