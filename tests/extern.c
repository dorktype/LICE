extern int external_1;
extern int external_2;

int main() {
    init("external storage");

    expecti(1337, external_1);
    expecti(7331, external_2);

    return ok();
}
