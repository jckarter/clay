_Complex float foof();
_Complex double foo();
_Complex long double fool();

struct bar;
struct opaque;

typedef int (*noproto_t)();
typedef int (*proto_t)(char x);
typedef int (*variadic_t)(char x, ...);
typedef int (* __attribute__((stdcall)) stdcall_t)(char x);

typedef int (*fparg)(char (*)(float));

typedef int (*(*fpret)(char))(float);

int (*fpretf(char))(float);

int barpf(struct bar *x);
int barf(struct bar x);

int opaquepf(struct opaque *x);
int opaquef(struct opaque x);

struct bar {
    int (*noproto)();
    int (*proto)(char x);
    int (*variadic)(char x, ...);
};

int asmnamed(int x) __asm__("foo");
int __attribute__((stdcall)) stdcalled(int x);

enum plain_enum_t { APPLE, BANANA, CHERRY };
enum valued_enum_t { STRAWBERRY = -1, VANILLA = 7 + 2, CHOCOLATE = 88 };
enum big_enum_t { KIWI, PASSIONFRUIT, forceLongLong = 0x7fffffffffffffffULL };
