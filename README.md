## Тести

### Всі

    $ ./build_all.sh
    $ ./test_all.sh

### Один

    $ cmake --build _build --target machina_control_test
    $ ./_bin/machina_control_test

### Один в CTest-режимі

    $ ctest --test-dir _build -R '^machina_control_test$' --output-on-failure
