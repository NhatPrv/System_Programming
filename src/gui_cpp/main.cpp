// Minimal ImGui + GLFW + GLAD example that reads POSIX shared memory and
// provides simple control buttons. This is a skeleton to start from.

// Standard / POSIX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string>

#include "../shared.h"

// GLAD must be included BEFORE glfw3.h to prevent system GL headers collision
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

static const char* glsl_version = "#version 130";

// Read shared memory (non-destructive, safe for demo). Returns false if not available.
bool read_shared(Shared*& out_shm, size_t& map_size) {
    const char* name = SHM_NAME;
    int fd = shm_open(name, O_RDONLY, 0);
    if (fd < 0) return false;
    struct stat st;
    if (fstat(fd, &st) == -1) { close(fd); return false; }
    map_size = st.st_size;
    void* ptr = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (ptr == MAP_FAILED) return false;
    out_shm = reinterpret_cast<Shared*>(ptr);
    return true;
}

int main(int, char**) {
    if (!glfwInit()) return 1;
    // GL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Group 11", NULL, NULL);
    if (window == NULL) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // Increase font size: add default font with larger size
    float baseFontSize = 20.0f; // increased from default ~13
    io.Fonts->AddFontDefault();
    ImFontConfig cfg; cfg.SizePixels = baseFontSize; // override size
    io.Fonts->AddFontDefault(&cfg);
    ImGui::StyleColorsLight();
    // Scale UI metrics a bit to match bigger font
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.15f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool show_demo = false;
    char shm_name[128];
    strncpy(shm_name, SHM_NAME, sizeof(shm_name));

    Shared* shm = nullptr;
    size_t shm_map_size = 0;

    // File paths (relative to running from gui_cpp/build). Both files live in src/
    const std::string src_dir = "../../"; // grandparent directory: src/
    const std::string input_path = src_dir + "input.txt";
    const std::string output_path = src_dir + "output.txt";

    // Buffers for input/output display
    static bool input_loaded = false;
    static std::string input_content;
    static char input_edit_buffer[8192];
    static std::string output_content;
    static time_t last_output_mtime = 0;
    static std::string status_msg;
    static ImVec4 status_color(0,0.4f,0,1);
    static bool open_save_popup = false;

    // Fixed layout constants (status bar height tied to font size for responsiveness)
    const float kSplitRatio = 0.58f;     // Left:Right width ratio (fixed)
    const float kHeaderH    = 0.0f;      // Reserved extra header height (px)

    auto load_file_string = [](const std::string& path) -> std::string {
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return std::string();
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz < 0) { fclose(f); return std::string(); }
        std::string data; data.resize((size_t)sz);
        if (sz > 0) fread(&data[0], 1, (size_t)sz, f);
        fclose(f);
        return data;
    };

    auto save_file_string = [](const std::string& path, const std::string& data) -> bool {
        FILE* f = fopen(path.c_str(), "wb");
        if (!f) return false;
        size_t written = fwrite(data.data(), 1, data.size(), f);
        fclose(f);
        return written == data.size();
    };

    auto refresh_input_if_needed = [&]() {
        if (!input_loaded) {
            input_content = load_file_string(input_path);
            if (input_content.size() >= sizeof(input_edit_buffer)) input_content.resize(sizeof(input_edit_buffer)-1);
            memset(input_edit_buffer, 0, sizeof(input_edit_buffer));
            memcpy(input_edit_buffer, input_content.c_str(), input_content.size());
            input_loaded = true;
        }
    };

    auto refresh_output = [&]() {
        output_content = load_file_string(output_path);
    };

    refresh_output(); // initial attempt

    // Helper to resolve executable paths (search CWD, ../, ../../)
    auto find_exe = [](const char* name) -> std::string {
        if (access(name, F_OK) == 0) return std::string(name);
        std::string up = std::string("../") + name; if (access(up.c_str(), F_OK) == 0) return up;
        std::string up2 = std::string("../../") + name; if (access(up2.c_str(), F_OK) == 0) return up2;
        return std::string(name);
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

    // Fullscreen main window: fills entire viewport, no titlebar/padding
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("SHM File Tool", nullptr, winFlags);

        // Top controls bar
        ImGui::Text("SHM: %s", SHM_NAME);
        if (shm) {
            ImGui::SameLine();
            ImGui::Text("  |  Address: %p", (void*)shm);
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh SHM")) {
            if (shm) { munmap(shm, shm_map_size); shm = nullptr; }
            if (!read_shared(shm, shm_map_size)) { shm = nullptr; }
        }
        ImGui::Separator();

        // Primary control buttons
        if (ImGui::Button("Start Writer")) {
            std::string exe = find_exe("writer");
            std::string cmd = exe + " -i " + input_path + " -n /shm_file_demo &";
            system(cmd.c_str());
            status_msg = "Writer started"; status_color = ImVec4(0,0.5f,0,1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Start Reader")) {
            std::string exe = find_exe("reader");
            std::string cmd = exe + " -o " + output_path + " -n /shm_file_demo -w 10 &";
            system(cmd.c_str());
            status_msg = "Reader started"; status_color = ImVec4(0,0.5f,0.5f,1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cleanup SHM")) {
            std::string exe = find_exe("cleanup");
            std::string cmd = exe + " /shm_file_demo 2>/dev/null";
            system(cmd.c_str());
            status_msg = "Cleanup executed"; status_color = ImVec4(0.6f,0,0,1);
        }

        // Optional SHM snapshot
        if (ImGui::CollapsingHeader("Shared Memory Snapshot", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (shm) {
                ImGui::Text("in=%zu  out=%zu", shm->in, shm->out);
                ImGui::BeginChild("shm_view", ImVec2(0, 120), true);
                for (size_t i = 0; i < CAP; ++i) {
                    ImGui::Text("[%zu] %s", i, shm->buf[i]);
                }
                ImGui::EndChild();
            } else {
                ImGui::TextColored(ImVec4(1,0,0,1), "No SHM or cannot read (start writer first)");
            }
        }

        ImGui::Separator();

        // Central fixed-ratio split panes
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float left_w = avail.x * kSplitRatio;
        float right_w = avail.x - left_w - 6.0f; // spacing
    float statusBarH = ImGui::GetFontSize() * 1.4f; // dynamic status bar height
    float panes_h = avail.y - statusBarH - kHeaderH;
        if (panes_h < 100.f) panes_h = 100.f;

        // Left pane (Input editor)
        ImGui::BeginChild("left_input", ImVec2(left_w, panes_h), true);
            ImGui::Text("input.txt");
            ImGui::SameLine();
            if (ImGui::SmallButton("Reload")) { input_loaded = false; refresh_input_if_needed(); status_msg = "Reloaded input.txt"; status_color = ImVec4(0,0.4f,0.8f,1); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Save")) {
                input_content = std::string(input_edit_buffer);
                bool ok = save_file_string(input_path, input_content);
                status_msg = ok ? "Saved input.txt" : "Failed to save input.txt";
                status_color = ok ? ImVec4(0,0.5f,0,1) : ImVec4(1,0,0,1);
                open_save_popup = true; ImGui::OpenPopup("Save Result");
            }
            ImGui::Separator();
            refresh_input_if_needed();
            ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
            ImVec2 edit_sz = ImGui::GetContentRegionAvail();
            ImGui::InputTextMultiline("##input", input_edit_buffer, sizeof(input_edit_buffer), edit_sz, flags);
        ImGui::EndChild();

        ImGui::SameLine();

        // Right pane (Output viewer)
        ImGui::BeginChild("right_output", ImVec2(right_w, panes_h), true);
            ImGui::Text("output.txt");
            ImGui::SameLine();
            if (ImGui::SmallButton("Reload")) { refresh_output(); status_msg = "Reloaded output.txt"; status_color = ImVec4(0.5f,0.3f,0.8f,1); }
            ImGui::Separator();
            ImGui::BeginChild("output_view", ImGui::GetContentRegionAvail(), true);
                ImGui::PushTextWrapPos();
                if (output_content.empty()) ImGui::TextColored(ImVec4(0.7f,0,0,1), "(output.txt empty)");
                else ImGui::TextUnformatted(output_content.c_str());
                ImGui::PopTextWrapPos();
            ImGui::EndChild();
        ImGui::EndChild();

        // Save result popup (modal)
        if (ImGui::BeginPopupModal("Save Result", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", status_msg.c_str());
            if (ImGui::Button("OK")) { ImGui::CloseCurrentPopup(); open_save_popup = false; }
            ImGui::EndPopup();
        }

        // Status bar (fixed height)
        ImGui::Separator();
    float statusBarH2 = ImGui::GetFontSize() * 1.4f;
    ImGui::BeginChild("status_bar", ImVec2(0, statusBarH2), false, ImGuiWindowFlags_NoScrollbar);
            if (!status_msg.empty()) ImGui::TextColored(status_color, "%s", status_msg.c_str());
            else ImGui::TextDisabled("Ready");
        ImGui::EndChild();

    ImGui::End();

        if (show_demo) ImGui::ShowDemoWindow(&show_demo);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
    // White background
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    if (shm) munmap(shm, shm_map_size);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
