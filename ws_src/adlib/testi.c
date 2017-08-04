static unsigned char k = 10;

int A(unsigned short a)
{
    return (63 - ((63-(k&63)) * a / 63));
}
int B(unsigned short a)
{
    return (63 + (k&63) * a / 63 - a);
}

int main(void)
{
    return A(5) + B(5);
}
