# lib

Загальні утиліти (все, окрім роботи з зображеннями). Зокрема, робота з файлами і шляхами, рядками,
логуванням, запуском shell-команд і простим вимірюванням часу.

## Файли

- `execlib.h/.cpp` - запуск зовнішньої команди через `popen`.
- `filelib.h/.cpp` - допоміжні функції для шляхів, файлів і директорій.
- `strlib.h/.cpp` - базові операції з рядками і printf-style логування.
- `csvlib.h/.cpp` - CSV escaping і форматування bool-значень для CSV.
- `mathlib.h/.cpp` - дрібні математичні helpers, зокрема 2D-вектори.
- `time.h/.cpp` - timestamp helpers і накопичення статистики часу виконання.

## execlib

### `bool exec(const std::string& cmd, std::string* result = nullptr)`

Запускає shell-команду `cmd` через `popen` і читає її stdout.

- `cmd` - команда для виконання.
- `result` - якщо не `nullptr`, stdout додається в цей рядок без додаткової обробки; якщо `nullptr`, кожен прочитаний рядок trim-иться і непорожні рядки друкуються в stdout.
- Результат: `true`, якщо pipe відкрито і читання завершено; `false`, якщо pipe не відкрився.

Обмеження: stderr окремо не читається; код завершення команди не аналізується.

## filelib

### `std::string nameOfFile(const std::string& filepath)`

Повертає ім'я файла з Unix-style шляху.

- `filepath` - шлях або ім'я файла.
- Результат: частина після останнього `/`; якщо `/` немає, повертається весь `filepath`.

### `std::string baseOfFile(const std::string& filepath)`

Повертає ім'я файла без останнього розширення.

- `filepath` - шлях або ім'я файла.
- Результат: `nameOfFile(filepath)` без частини після останньої крапки; якщо крапки немає, повертається ім'я файла.

### `std::string pathOfFile(const std::string& filepath)`

Повертає директорію з Unix-style шляху.

- `filepath` - шлях до файла.
- Результат: частина до і включно з останнім `/`; якщо `/` немає, повертається порожній рядок.

### `std::string extOfFile(const std::string& filepath)`

Повертає розширення файла без крапки.

- `filepath` - шлях або ім'я файла.
- Результат: частина після останньої крапки в імені файла; якщо крапки немає, повертається порожній рядок.

### `std::string extPartOfFile(const std::string& filepath)`

Повертає розширення файла разом із крапкою.

- `filepath` - шлях або ім'я файла.
- Результат: частина від останньої крапки в імені файла; якщо крапки немає, повертається порожній рядок.

### `bool clearFile(const std::string& filepath)`

Відкриває файл у режимі `"w"` і тим самим очищає або створює його.

- `filepath` - шлях до файла.
- Результат: `true`, якщо файл відкрився, був flush-нутий і закритий; `false` при помилці відкриття.

### `std::string newPath(const std::string& path)`

Створює директорію з усіма батьківськими директоріями.

- `path` - шлях до директорії.
- Результат: нормалізований шлях із кінцевим `/`; порожній рядок, якщо `path` порожній або створення директорій впало з exception.

### `FILE* newFile(const std::string& filepath)`

Відкриває файл для запису.

- `filepath` - шлях до файла.
- Результат: `FILE*` у режимі `"w"` або `nullptr` при помилці.

### `int findExtension(std::string path, const std::string* exts, int extsCnt)`

Шукає у директорії перший файл, ім'я якого закінчується одним із заданих суфіксів.

- `path` - директорія; якщо порожня, використовується `"./"`.
- `exts` - масив суфіксів/розширень для перевірки.
- `extsCnt` - кількість елементів у `exts`.
- Результат: індекс першого збігу в `exts`; `-1`, якщо збігів немає, `extsCnt <= 0` або директорію не вдалося відкрити.

### `bool hasSubdirs(std::filesystem::path path)`

Перевіряє, чи має директорія піддиректорії.

- `path` - шлях до директорії.
- Результат: `true`, якщо знайдено хоча б одну піддиректорію, крім `.` і `..`; `false`, якщо шлях не є директорією, директорію не відкрито або піддиректорій не знайдено.

### `std::list<std::string> listOfFiles(const std::string& path, const std::string* exts, int extsCnt, bool dirs = true, bool files = true, const std::string& prefix = "")`

Повертає відсортований список імен елементів директорії.

- `path` - директорія для читання.
- `exts` - масив дозволених суфіксів/розширень; ігнорується, якщо `extsCnt <= 0`.
- `extsCnt` - кількість елементів у `exts`.
- `dirs` - чи включати директорії.
- `files` - чи включати звичайні файли.
- `prefix` - необов'язковий префікс імені.
- Результат: відсортований список імен без `path`; порожній список при помилці відкриття або відсутності збігів.

### `bool readFileByLines(const std::string& filepath, std::vector<std::string>& lines)`

Читає текстовий файл рядок за рядком.

- `filepath` - шлях до файла.
- `lines` - вектор, у який додаються прочитані рядки; перед читанням не очищається.
- Результат: `true`, якщо файл відкрито і прочитано; `false` при помилці відкриття.

### `bool readFile(const std::string& filepath, std::string& text)`

Читає весь текстовий файл у рядок.

- `filepath` - шлях до файла.
- `text` - вихідний рядок; очищається перед читанням, кожен рядок додається з `\n`.
- Результат: `true`, якщо файл відкрито і прочитано; `false` при помилці відкриття.

### `bool writeFile(const std::string& filepath, const char* modes, const char* content)`

Відкриває файл через `fopen` і записує C-рядок.

- `filepath` - шлях до файла.
- `modes` - режим `fopen`, наприклад `"w"` або `"a"`.
- `content` - текст для запису; якщо `nullptr`, файл відкривається, але функція повертає `true` без запису.
- Результат: `true`, якщо файл відкрито і кількість записаних символів дорівнює `strlen(content)`; `false` при помилці відкриття або неповному записі.

### `bool renamePath(const std::string& oldPath, const std::string& newPath)`

Перейменовує або переміщує файл/директорію через `std::filesystem::rename`.

- `oldPath` - поточний шлях.
- `newPath` - новий шлях.
- Результат: `true` при успішному rename; `false`, якщо `std::filesystem` кинув `filesystem_error`.

## strlib

### `std::vector<std::string> split(std::string s, const std::string& delimiter)`

Розбиває рядок на частини за роздільником.

- `s` - рядок для розбиття; передається за значенням і змінюється всередині функції.
- `delimiter` - рядок-роздільник, який шукається через `find`.
- Результат: вектор частин. Поточна реалізація після знайденого delimiter відкидає один символ (`pos + 1`), тому коректно поводиться як single-character delimiter.

### `std::string tail(std::string const& src, size_t const length)`

Повертає останні `length` символів рядка.

- `src` - вихідний рядок.
- `length` - бажана довжина хвоста.
- Результат: весь `src`, якщо `length >= src.size()`, інакше `src.substr(src.size() - length)`.

### `void trim(std::string& str)`

Обрізає whitespace на початку і в кінці рядка.

- `str` - рядок, який змінюється на місці.
- Результат: немає; `str` стає очищеним від символів із `WHITESPACE`, або порожнім, якщо складався лише з whitespace.

### `void logger(FILE* fLog, const char* format, ...)`

Друкує printf-style повідомлення в stdout і, якщо задано, у файл.

- `fLog` - файл для дублювання повідомлення; може бути `nullptr`.
- `format` - форматний рядок як у `printf`.
- `...` - аргументи для форматного рядка.
- Результат: немає.

## csvlib

### `std::string csvEscape(const std::string& s)`

Екранує рядок для CSV.

- `s` - вхідний рядок.
- Результат: `s` без змін, якщо в ньому немає коми, лапок або переносу рядка; інакше рядок у подвійних лапках із продубльованими внутрішніми лапками.

### `std::string boolCsv(bool v)`

Форматує bool-значення для CSV.

- `v` - bool-значення.
- Результат: `"1"` для `true`, `"0"` для `false`.

## time

### `std::string formatTimeCustom(time_t t)`

Форматує час у локальному часовому поясі.

- `t` - timestamp у форматі `time_t`.
- Результат: рядок формату `YYYY-MM-DDTHH:MM:SS`.

### `moment now()`

Повертає поточний час `CLOCK_REALTIME` у наносекундах.

- Параметри: немає.
- Результат: `uint64_t` nanosecond timestamp.

### `uint64_t nowMs()`

Повертає поточний час `CLOCK_REALTIME` у мілісекундах.

- Параметри: немає.
- Результат: `uint64_t` millisecond timestamp.

### `Timing::Timing(uint afterCnt = 0, uint resetEachCnt = 0)`

Створює накопичувач статистики часу.

- `afterCnt` - якщо лічильник досягає цього значення, статистика скидається.
- `resetEachCnt` - збережений параметр для періодичного reset, але поточна реалізація його не обробляє.
- Результат: об'єкт `Timing` із нульовою статистикою.

### `void Timing::add(moment started_at)`

Додає один вимір часу.

- `started_at` - timestamp початку, зазвичай отриманий через `now()`.
- Результат: немає; оновлює `cnt`, `sum`, `min`, `max`, або скидає статистику при досягненні `afterCnt`.

### `timing_stat Timing::get()`

Повертає поточну статистику.

- Параметри: немає.
- Результат: `timing_stat { cnt, avg, min, max }`, де значення часу в наносекундах; `avg` дорівнює `0`, якщо `cnt == 0`.

### `void Timing::show(const std::string& label, bool showFPS = false, FILE* fLog = nullptr)`

Друкує статистику часу у stdout і за потреби у файл.

- `label` - назва вимірюваної операції.
- `showFPS` - якщо `true`, додає розрахунок FPS як `SECOND / avg`.
- `fLog` - файл для дублювання виводу; може бути `nullptr`.
- Результат: немає.

### `timing_stat`

Структура статистики часу.

- `cnt` - кількість накопичених вимірів.
- `avg` - середня тривалість у наносекундах.
- `min` - мінімальна тривалість у наносекундах.
- `max` - максимальна тривалість у наносекундах.

### `bool ensureDirectory(const std::filesystem::path& path, const std::string& label, bool createIfMissing = true)`

Перевіряє, що `path` існує як директорія; якщо `createIfMissing == true`, створює її разом із батьківськими директоріями.
Помилки друкуються в stderr з префіксом `on ensureDirectory():`.

### `bool ensureNotRegularFile(const std::filesystem::path& path, const std::string& label)`

Перевіряє, що за шляхом `path` не лежить regular file, який заважатиме використати цей шлях як директорію.
Помилки друкуються в stderr з префіксом `on ensureNotRegularFile():`.

### `std::string safePathPart(std::string s)`

Замінює всі символи, крім літер, цифр, `.`, `_`, `-`, на `_`; якщо рядок порожній, повертає `"file"`.

### `bool sameFileSize(const std::filesystem::path& a, const std::filesystem::path& b, bool& same)`

Порівнює розмір двох файлів і записує результат у `same`.
Помилки друкуються в stderr з префіксом `on sameFileSize():`.

### `bool removeFsPath(const std::filesystem::path& path, const std::string& reason)`

Видаляє файл/шлях через `std::filesystem::remove`.
Помилки друкуються в stderr з префіксом `on removeFsPath():`.

### `bool moveFileReplacing(const std::filesystem::path& srcPath, const std::filesystem::path& dstPath)`

Переносить файл: спершу пробує `rename`, а якщо це не вдалося, виконує `copy_file(overwrite_existing)` і видаляє source.
Помилки друкуються в stderr з префіксом `on moveFileReplacing():`.

### `bool cleanupDirectory(const std::filesystem::path& dirPath)`

Видаляє директорію з усім вмістом через `std::filesystem::remove_all`.
Помилки друкуються в stderr з префіксом `on cleanupDirectory():`.

## mathlib

### `Vec2D` / `dot()` / `len()` / `normalized()` / `angleToHorizontalRad()`

Базові helpers для 2D-векторів і кутів. `normalized()` стабілізує напрямок вектора так, щоб він не дивився в ліву півплощину; `angleToHorizontalRad()` повертає гострий кут між вектором і горизонталлю.
