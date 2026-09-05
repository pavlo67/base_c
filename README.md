## Тести

### Всі

    $ cmake --build cmake-build-release
    $ ctest --test-dir cmake-build-release --output-on-failure

### Один

    $ cmake --build cmake-build-release --target machina_control_test
    $ ./_bin/machina_control_test

### Один в CTest-режимі

    $ ctest --test-dir cmake-build-release -R '^machina_control_test$' --output-on-failure
