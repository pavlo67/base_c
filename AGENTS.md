# Repository Instructions

## AGENTS.md

AGENTS.md в підмодулях (mod_.../) в Linux-системі — це hard links до кореневого AGENTS.md: в будь-якому випадку, не треба обробляти їх окремо, 
працюючи з основним проєктом.

Не слід вносити правки в AGENTS.md — коли вои потрібні, слід пропонувати їх автору, який внесе ці правки вручну. 


## Actions

Перед будь-якими змінами спочатку проаналізуй запит, опиши запропоновані дії та суттєві нюанси, після чого зупинись і дочекайся підтвердження.
Початковий запит і/або доповнення на кшталт «прошу про поправки», «дороби» тощо, не є таким підтвердженням. Дозвіл надається лише окремим
повідомленням користувача після обговорення: «виконуй», «так, зроби» або рівнозначною явною командою конкретно у відповідь на опис змін, наданих агентом.
Новий запит на поправки потребує нового підтвердження. Читання в межах запиту та CMake/CTest для вже погоджених змін дозволені без окремого підтвердження.

При аналізі кожного каталогу починай з прочитання відповідного info.md.

Prefer existing repository patterns over new conventions.  

Always look for helper functions in lib/ and hardware/ first. 

Before finalizing, move all new reusable helpers from app/local files to `lib/` or `hardware/`. Treat string conversion, math, filesystem, and formatting
helpers as reusable by default.

Для функціональних директорій підтримуй стислий info.md: призначення, основні точки входу, нетривіальна логіка та обмеження 
(зокрема, хто володіє станом процесу; правила синхронізації, виконання та зупинки), причини нетривіальних рішень. Оновлюй 
його, коли зміни роблять наявний опис неточним або додають важливу для розуміння поведінку. Не дублюй очевидні 
декларації та інформацію з інших info.md; натомість використовуй посилання.

Do not show diffs.


## Coding style

All class member field names must end with an underscore, e.g. `original_`.

Never create overloaded functions or methods. Every function or method must have a unique name within its scope, 
regardless of differences in parameter types, parameter count, qualifiers, or return type. If two operations require 
different signatures, give them different, descriptive names.

Guard platform-specific code, including its headers, with platform preprocessor macros so that it is active only on its target platform.

Кожне повідомлення про помилку повинно містити однозначний контекст у форматі `[function()] ERROR: ...` або
`[Class.function()] ERROR: ...`. Якщо контекст повторюється в кількох повідомленнях функції, визначати спільну
константу перед нею; для одного повідомлення дозволено записувати контекст безпосередньо в літералі.

Wherever possible, use error codes/messages instead of throwing exceptions.

Don't use stderr (`std::cerr`); send all error messages to stdout. Error messages must follow the format `<context> ERROR: <details>`.

The `main()` function in `main.cpp` should appear first — after includes, constants, types, static variables, and helper 
declarations (without bodies), of course.

Don't use CLI parameters in apps unless explicitly required by the task — define all parameters as constants in main.cpp.

For every new function, ask: - Is it's not tightly coupled to one function and reusable outside this file? If the answer is yes, 
move it to `lib/` or `hardware/` as appropriate. In particular, CSV/text helpers, bool/string conversion helpers, math helpers, 
filesystem helpers, and small formatting helpers are reusable and should be moved to common directories by default unless proven otherwise.

Compile definitions не слід визначати в CMakeLists.txt, за винятком тих, значення яких генерує сам cmake в процесі виконання. 
Коли потрібна wide-range константа, її місце — в defines_<PROJECT_ALIAS>.h і в defines_<PROJECT_ALIAS>_example.h в корені проєкту.  

Wrap all single-statement `if`/`for`/`while` bodies in curly braces, for example:

    if (<CONDITION>) { continue; }

#if слід розміщати з "поточним" відступом, а його вміст — зі стандартним "відступом + 1". наприклад:

    while (true) {
        action1()
        #if CONDITION
            action2()
        #endif
    }

Скрізь, де "return 0/1" означають успіх/невдачу, пишемо, відповідно: "return EXIT_SUCCESS;" або "return EXIT_FAILURE;"

## Configuration

Конфіги лежать в корені проєкту і кожного підмодуля з назвою machina.yaml. 

Конфіг проєкту містить ключі/секції з конфігів усіх підмодулів, конфіг підмодуля — тілкьи свої.

Там же лежать аналогічні файли-приклади: machina.yaml_example (приклади зберігаються в репо, конфіги — ні).

Зчитування секцій конфігу робиться відповідними хелперами, як то: mod_base/hardware/stepper_motor_config::loadPlatformMotorConfig()

В застосунках, пробниках і тестах шлях до конфігу вказується відносний: "machina.yaml" (конфіг з каталогу запуску), самі 
конфіги не слід при цьому копіювати в каталог, де знаходяться відповідні програмні коди. 


## Testing

Write all tests with GTest and CTest

Use only fatal assertions in tests (ASSERT_... in GTest, not EXPECT_..)

Add concise test logs showing the sequence of steps. Use distinct short prefixes to identify output from different goroutines or applications.


## Probes

Кожен пробник (застосунок, які потрапляює в _probe) має містити весь свій істотний код у власному main() — за винятком ініціялізації
та інших бібліотечних операцій, зокрема бібліотек, які перевіряємо (їх код, якраз, не повинен дублюватись у пробниках).
