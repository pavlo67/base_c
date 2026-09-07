Якщо функція (будь-яка окрім main()) виводить помилку або передає текст щодо помилки назовні (через параметр чи exception), 
то безпосередньо перед функцією (а не на початку файлу!) має бути описана константа, яка додається в усі ті виводи. в 
константі використовується імʼя функції, якщо воно унікальне або "імʼя обʼєкта/підкаталогу/контексту" dot "імʼя функції", якщо воно не унікальне або є 
універсальним терміном на кшталт move(). тобто "on requiredDirectory()", але "on <контекст>.move()"     

    const std::string ON_REQUIRED_DIRECTORY = "on requiredDirectory()"

    static bool requireDirectory(const fs::path& path, const std::string& label, bool createIfMissing) {
    std::error_code ec;
    
        if (!fs::exists(path, ec)) {
            if (ec) {
                std::cout << "ERROR: " << ON_REQUIRED_DIRECTORY << "failed to check " << label << ": " << path << ": " << ec.message() << "\n";
                return false;
            }
            ... 

