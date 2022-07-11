// Online C++ compiler to run C++ program online
#include <iostream>
#include <memory>

class A;

//std::shared_ptr<A> pointy;
std::weak_ptr<A> pointw;

class A : public std::enable_shared_from_this<A> {
public:
    int i;
    A() : std::enable_shared_from_this<A>() {
        //pointy = shared_from_this();
        pointw = weak_from_this();
    }
};

int main() {
    // Write C++ code here
    std::shared_ptr<A> a = std::make_shared<A>();

    std::cout << "Hello" << std::endl;

    a->i = 89;

    std::cout << a->i << std::endl;

    /*{
        std::cout << pointy->i << std::endl;
        pointy->i = 59;
    }*/

    {
        std::shared_ptr<A> aw = pointw.lock();
        std::cout << aw->i << std::endl;
        aw->i = 78;
    }

    std::cout << a->i << std::endl;

    return 0;
}