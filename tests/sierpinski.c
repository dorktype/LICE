int main() {
    int size = 32; // change for larger triangles
    for (int y = size; y > 0; y--) {
        for (int i = 0; i < y; i++)
            putchar(' ');
        for (int x = 0; x + y < size; x++)
            printf((x & y) ? "  " : "* ");
        printf("\n");
    }
    return 0;
}
