typedef struct Struct4 { int x; } Struct4;
typedef struct Struct8 { Struct4 x; Struct4 y; } Struct8;
typedef struct Struct16 { Struct8 x; Struct8 y; } Struct16;
typedef struct Struct32 { Struct16 x; Struct16 y; } Struct32;
typedef struct Struct64 { Struct32 x; Struct32 y; } Struct64;

Struct4  foo4 (int x)                  { return (Struct4 ){x};    }
Struct8  foo8 (Struct4  x, Struct4  y) { return (Struct8 ){x, y}; }
Struct16 foo16(Struct8  x, Struct8  y) { return (Struct16){x, y}; }
Struct32 foo32(Struct16 x, Struct16 y) { return (Struct32){x, y}; }
Struct64 foo64(Struct32 x, Struct32 y) { return (Struct64){x, y}; }

void bar4 (Struct4  *r, int x)                  { *r = (Struct4 ){x};    }
void bar8 (Struct8  *r, Struct4  x, Struct4  y) { *r = (Struct8 ){x, y}; }
void bar16(Struct16 *r, Struct8  x, Struct8  y) { *r = (Struct16){x, y}; }
void bar32(Struct32 *r, Struct16 x, Struct16 y) { *r = (Struct32){x, y}; }
void bar64(Struct64 *r, Struct32 x, Struct32 y) { *r = (Struct64){x, y}; }
