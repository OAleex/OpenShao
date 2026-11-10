#include "issue_link.h"
#include <cstdio>

static int failures = 0;
static void Check(bool value, const char* message) {
    if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}

int main() {
    using namespace openshao::support;
    Check(PercentEncode("A b&c=d\n%") == "A%20b%26c%3Dd%0A%25", "reserved characters are percent-encoded");
    Check(PercentEncode("Sobreviv\xC3\xAAncia") == "Sobreviv%C3%AAncia", "UTF-8 bytes are encoded individually");

    openshao::launcher::Settings settings;
    settings.renderer = "vulkan";
    settings.width = 1920;
    settings.height = 1080;
    settings.aspect_ratio = "16:9";
    settings.language = "pt-BR";
    const auto environment = IssueEnvironment(settings, "0.5.0", "Windows");
    Check(environment.starts_with("OpenShao 0.5.0 (Windows)\n"), "environment starts with version and platform");
    Check(environment.find("Renderer: Vulkan\n") != std::string::npos, "renderer uses its display name");
    Check(environment.find("1920 x 1080, aspect 16:9") != std::string::npos, "display size and aspect ratio are listed");
    Check(environment.find("Menu language: pt-BR") != std::string::npos, "menu language is listed");

    settings.width = settings.height = 0;
    Check(IssueEnvironment(settings, "0.5.0", "Linux").find("desktop x desktop") != std::string::npos,
          "automatic resolution is reported as the desktop size");

    const auto bug = IssueLink(IssueKind::Bug, "line 1\nline 2");
    Check(bug == "https://github.com/OAleex/OpenShao/issues/new?template=bug_report.yml&environment=line%201%0Aline%202",
          "bug link selects the bug form and fills the environment field");
    Check(IssueLink(IssueKind::Crash, "").find("template=crash.yml&environment=") != std::string::npos,
          "crash link selects the crash form");

    std::printf("Issue link: %d failures\n", failures);
    return failures != 0;
}
