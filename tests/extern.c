extern int external_1;
extern int external_2;

int main() {
    init("external storage");

    //expecti(external_1, 1337);
    //expecti(external_2, 7331);

    return ok();
}
