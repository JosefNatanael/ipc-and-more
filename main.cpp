#include <iostream>
#include <ostream>
#include <iosfwd>
#include <vector>
#include <type_traits>
#include <fstream>
#include <functional>

#define JOSEF 234
#include "main.h"


template<typename T>
concept IsInt = std::is_same<T, int>::value;

template<typename T>
struct Foo {
    void foo() {
        std::cout << "foo\n";
    }

    template<typename U = T>
    typename std::enable_if_t<std::is_same<U, int>::value, void> bar() {
        std::cout << "bar\n";
    }

    template<IsInt U = T>
    void baz() {
        std::cout << "baz\n";
    }
};

struct Ops1 {
    int a;

    friend bool operator==(const Ops1& lhs, const Ops1& rhs) {  // friend is needed to access private members
        return lhs.a == rhs.a;
    }
};

struct Ops2 {
    int a;

    bool operator==(const Ops1& other) const {
        return this->a == other.a;
    }
};

struct X {
    int i;
    double d;
};

inline std::ostream& operator<<(std::ostream& os, const X& x) {
    return os << x.i << " " << x.d;
}

inline std::istream& operator>>(std::istream& os, X& x) {
    char c;
    // this means to
    if (os >> x.i >> c >> x.d && c != ',') {
        os.setstate(std::ios_base::failbit);
    }
    return os;
}

#define STR(X) static_cast<std::ostringstream&&>(std::ostringstream() << X).str()
#define Hihi(a, op)
#define LOG(msg) std::cout << __FILE__ << ":" << __LINE__ << " " << msg << std::endl;


struct Sample {
    mutable int a;
    void setter(int newa) const {
        a = newa;
    }
};

void test1(int& n) {
    *reinterpret_cast<unsigned*>(&n) /= 2;
}

int main() {
    LOG("Hello");
    Foo<int> f;
    LOG("Heyy");
    Foo<std::string> g;
    f.bar();
    f.foo();
    f.baz();
    g.foo();
    // g.bar(); => not allowed
    // g.baz(); => not allowed

    bool result1 = Ops1{1} == Ops1{1};
    // bool result2 = Ops2{1} == Ops2{1}; // not allowed

//    X obj;
//    std::cin >> obj;
//    if (std::cin.fail()) {
//        std::cout << "fail\n";
//    } else {
//        std::cout << obj << '\n';
//    }

//    if (std::ifstream in{"../scratch.txt"}) {
//        while (!in.eof()) {
//            std::string line;
//            std::getline(in, line);
//            std::cout << line << '\n';
//        }
//    } else {
//        std::cerr << "File not found\n";
//    }

    Hihi(ok, --)
    const Sample sample{};
    sample.setter(2);

    std::function<void()> try_dangle;
    {
        int x = 420;
        try_dangle = [&x] {
            std::cout << x << std::endl;
        };
    }
    try_dangle();

    int n = 10;
    test1(n);
    std::cout << n << std::endl;

    return JOSEF;
}
