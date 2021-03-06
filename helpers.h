#ifndef TOV8_HELPERS
#define TOV8_HELPERS  

  #include "type_literal.h" // TYPE_LITERAL macro for making aligned type string for boxing pointers


#if (NODE_MODULE_VERSION > 0x000B)
#define CBIND_UV_ASYNC_ARGS uv_async_t* handle
#else
#define CBIND_UV_ASYNC_ARGS uv_async_t* handle, int status
#endif

  class v8_exception : public std::exception
  {
    std::string msg;
  public:
    v8_exception(const std::string& _msg) : msg(_msg) {
    }
    virtual const char* what() const throw() {
      return msg.c_str();
    }
  };

  
  template<class T>
  class AsyncFunctionWrap : public NanAsyncWorker {
    std::function<T()> func;
  public:
    AsyncFunctionWrap(std::function<T()> _func) : func(_func) {

    }
    virtual void Execute() final {
      T retValue = func();

    }


  };

  template<class T>
  class FunctionWrap : public node::ObjectWrap {
    std::function<void(T)> deleter;
    T m_handle = nullptr;
    FunctionWrap(T handle, std::function<void(T)> _deleter): deleter(_deleter), m_handle(handle) {

    }
    static void wrap(v8::Local<v8::Object>& obj, T in, std::function<void(T)> deleter = [](T){} ) {
      (new FunctionWrap<T>(in, deleter))->Wrap(obj);
    }
  public:

    static T nativeFromV8(v8::Handle<v8::Value> obj, const char* typeId, v8::Persistent<v8::Function>& persistent, T cbNative) {
      if(!obj->IsFunction())
        throw v8_exception("Argument needs to be a Function");
      ;
      NanAssignPersistent(persistent, obj.As<v8::Function>());

      return cbNative;
    }

    static v8::Handle<v8::Function> create(const char* typeId, T in, NanFunctionCallback cb, std::function<void(T)> deleter = [](T){} ) {
      v8::Local<v8::FunctionTemplate> funTpl = NanNew<v8::FunctionTemplate>(cb);

      v8::Local<v8::Function> fun = funTpl->GetFunction();

      v8::Local<v8::Object> prototype = fun->GetPrototype().As<v8::Object>();

      // this can be cached
      v8::Local<v8::Function> bind = prototype->Get(NanNew<v8::String>("bind")).As<v8::Function>();
      
      v8::Local<v8::ObjectTemplate> objTpl = v8::ObjectTemplate::New();
      objTpl->SetInternalFieldCount(1);
      v8::Local<v8::Object> obj = objTpl->NewInstance();

      wrap(obj, in, deleter);

      v8::Handle<v8::Value> argv[1] = {obj};

      // note this can be much improved in terms of performance, for example
      // by using internal fields with pointers
      fun = NanMakeCallback(fun, bind, 1, argv).As<v8::Function>();

      fun->Set(NanNew<v8::String>("__tov8_wrapped_data"), obj);

      fun->Set(NanNew<v8::String>("nativeInterface"), NanNew<v8::String>(typeId));

      return fun;
    }
    void clear_free() {
      m_handle = nullptr;
      deleter = nullptr;
    }
    virtual ~FunctionWrap() {
      if(deleter)
        deleter(m_handle);
    }

    static T Unwrap(v8::Handle<v8::Object> obj, const char* typeId = NULL) {

      return node::ObjectWrap::Unwrap<FunctionWrap<T>>(obj)->get();
    }


    T get() const {
      return m_handle;
    }
  };


  template<class T>
  class HandleWrap : public node::ObjectWrap {
    std::function<void(T)> deleter;
    T m_handle;

    static void wrap(v8::Local<v8::Object>& obj, T in, std::function<void(T)> deleter = [](T){} ) {
      (new HandleWrap<T>(in, deleter))->Wrap(obj);
    }
  public:
    HandleWrap(T handle, std::function<void(T)> _deleter): deleter(_deleter), m_handle(handle) {

    }


    static v8::Handle<v8::Object> createSafe(const char* typeString, T in, std::function<void(T)> deleter = [](T){} ) {
      v8::Local<v8::ObjectTemplate> objTpl = v8::ObjectTemplate::New();
      objTpl->SetInternalFieldCount(2);

      v8::Local<v8::Object> obj = objTpl->NewInstance();

      NanSetInternalFieldPointer(obj, 1, (void*)typeString);

      wrap(obj, in, deleter);
      return obj;
    }

    static v8::Handle<v8::Object> create(T in, std::function<void(T)> deleter = [](T){} ) {
      return createSafe("", in, deleter);
    }

    void clear_free() {
      m_handle = nullptr;
      deleter = nullptr;
    }
    virtual ~HandleWrap() {
      if(deleter)
        deleter(m_handle);
    }

    static T Unwrap(v8::Handle<v8::Object> obj, const char* typeId = NULL) {
      void *typeStringPtr = (obj->InternalFieldCount() == 2)?NanGetInternalFieldPointer(obj, 1):nullptr;
      if(!typeStringPtr) {
        throw v8_exception("Unable to unbox pointer from object, not created by tov8");
      }
      if(typeId) {
        if(strcmp(typeId, (char*)typeStringPtr) != 0) {
          throw v8_exception(std::string("Unable to unbox pointer ") + typeId + " from pointer of type " + (const char*)typeStringPtr);
        }
      }
      return node::ObjectWrap::Unwrap<HandleWrap<T>>(obj)->get();
    }

    T get() const {
      return m_handle;
    }
  };

  template<typename T>
  struct auto_delete {
    T obj;
    auto_delete(T _obj) : obj(_obj) {}
    operator T() {
      return obj;
    }
    ~auto_delete() {
      delete obj;
    }
  };
  template<typename T>
  struct auto_deleteA {
    T obj;
    auto_deleteA(T _obj) : obj(_obj) {}
    operator T() {
      return obj;
    }
    ~auto_deleteA() {
      delete[] obj;
    }
  };
  template<typename T>
  struct auto_free {
    T obj;
    auto_free(T _obj) : obj(_obj) {}
    operator T() {
      return obj;
    }
    ~auto_free() {
     free((void*)obj);
    }
  };

// types
template<typename T>
  T getArgument() {
    return T();
  }




template<typename T>
  bool checkArgument(v8::Handle<v8::Value> obj) {
    return true;
  }
  template<typename T>
  T getArgument(v8::Handle<v8::Value> obj) {
    return T();
  }
  template<typename T>
  v8::Handle<v8::Value> toV8Type(T input) {
    return NanUndefined();
  }

#define cbind_declare_type(v8_type, type_name, type_identifier) \
  template<> \
  bool checkArgument<type_name>(v8::Handle<v8::Value> obj) { \
    if(!obj->CBIND_CONCAT(Is, v8_type)()) \
      return false; \
    return true; \
  } \
  template<> \
  type_name getArgument<type_name>(v8::Handle<v8::Value> obj) { \
    return obj->CBIND_CONCAT(v8_type, Value)(); \
  }  \
  v8::Handle<v8::Value> CBIND_CONCAT(toV8Type_,type_identifier)(type_name input) { \
    return NanNew<v8::v8_type>(input); \
  }

#define cbind_declare_type_alias(type_name, type_identifier_from, type_identifier) \
  v8::Handle<v8::Value> CBIND_CONCAT(toV8Type_,type_identifier)(type_name input) { \
    return CBIND_CONCAT(toV8Type_,type_identifier_from)(input); \
  }

  cbind_declare_type(Number, double, double)
  cbind_declare_type(Number, float, float)
  cbind_declare_type(Number, size_t, size_t)
  cbind_declare_type(Number, int, int)
  cbind_declare_type(Number, unsigned int, unsigned_int)
  cbind_declare_type(Boolean, bool, bool)

  cbind_declare_type_alias(int, int, int32_t)
  cbind_declare_type_alias(unsigned int, unsigned_int, uint32_t)


  // type string
  template<>
  bool checkArgument<std::string>(v8::Handle<v8::Value> obj) {
    if(!obj->IsString())
      return false;
    return true;
  }
  template<>
  std::string getArgument<std::string>(v8::Handle<v8::Value> obj) {
    size_t count;
    const char* str = NanCString(obj, &count);
    std::string out = std::string(str);
    delete[] str;
    return out;
  }
  
  v8::Handle<v8::Value> toV8Type_std__string(std::string input) {
    return NanNew<v8::String>(input.c_str());
  }

  // type const char*
  template<>
  bool checkArgument<const char*>(v8::Handle<v8::Value> obj) {
    if(!obj->IsString())
      return false;
    return true;
  }
  template<>
  const char* getArgument<const char*>(v8::Handle<v8::Value> obj) {
    size_t count;
    return NanCString(obj, &count);
  }
  
  v8::Handle<v8::Value> toV8Type_const_char_ptr(const char* input, std::function<void(const char*)> deleter = [](const char*){}) {
    if(!input) {
      return NanNull();
    }
    auto val = NanNew<v8::String>(input);
    deleter(input);
    return val;
  }

  template<>
  bool checkArgument<char*>(v8::Handle<v8::Value> obj) {
    if(!obj->IsString())
      return false;
    return true;
  }
  template<>
  char* getArgument<char*>(v8::Handle<v8::Value> obj) {
    size_t count;
    return NanCString(obj, &count);
  }
  
  v8::Handle<v8::Value> toV8Type_char_ptr(char* input, std::function<void(char*)> deleter = [](char*){}) {
    if(!input) {
      return NanNull();
    }
    auto val = NanNew<v8::String>(input);
    deleter(input);
    return val;
  }



#endif // TOV8_HELPERS