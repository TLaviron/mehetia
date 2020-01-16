#include <memory>

class MainApplication
{
public:
    MainApplication();
    ~MainApplication();

    MainApplication(const MainApplication&) = delete;
    MainApplication& operator=(const MainApplication&) = delete;

    void run();

    void cleanup();

private:
    class Pimpl;

    std::unique_ptr<Pimpl> m_pimpl;

};
