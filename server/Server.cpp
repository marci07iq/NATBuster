#include <iostream>
#include <utility>

struct Foo {
    int z = 5;
    void ff(int x, float y) {
        std::cout << z << ":" << x << " " << y << std::endl;
    }
};

//Type erasure
template <typename... ARGS>
class CallbackBase {
protected:
    CallbackBase() {

    }
public:
    virtual void operator()(ARGS&&... args) const = 0;
};

//Member function callback, via a weak pointer
template <typename CLASS, typename RET, typename... ARGS>
class MemberCallback : public CallbackBase<ARGS...> {
public:
    using fn_type = RET(CLASS::*)(ARGS...);
private:
    fn_type _fn;
    std::weak_ptr<CLASS> _inst;
public:

    MemberCallback(std::weak_ptr<CLASS> inst, fn_type fn) : _inst(inst), _fn(fn) {

    }
    inline void operator()(ARGS&&... args) const override {
        std::shared_ptr<CLASS> inst = _inst.lock();
        if (inst) {
            (inst.get()->*(_fn))(args...);
        }
    }
};

int main() {
    std::shared_ptr<Foo> foo = std::make_shared<Foo>();

    CallbackBase<int, float>* asd = new MemberCallback<Foo, void, int, float>(foo, &Foo::ff);


    asd->operator()(1, 2.3f);
}