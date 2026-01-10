#include "console/CommandHandlers.hpp"
#include "console/Command.hpp"
#include "console/Console.hpp"
#include <storm/String.hpp>
#include <cstring>

namespace {
    struct HelpCategory {
        CATEGORY category;
        const char* name;
    };

    const HelpCategory kHelpCategories[] = {
        { DEBUG,   "debug" },
        { GRAPHICS, "graphics" },
        { CONSOLE, "console" },
        { COMBAT,  "combat" },
        { GAME,    "game" },
        { DEFAULT, "default" },
        { NET,     "net" },
        { SOUND,   "sound" },
        { GM,      "gm" },
    };

    void AppendText(char* buffer, size_t bufferSize, const char* text) {
        if (!buffer || !text || bufferSize == 0) {
            return;
        }

        size_t len = SStrLen(buffer);
        if (len >= bufferSize - 1) {
            return;
        }

        SStrCopy(buffer + len, text, bufferSize - len);
    }
}

int32_t ConsoleCommand_Help(const char* command, const char* arguments) {
    (void)command;

    const char* args = arguments ? arguments : "";

    if (*args == '\0') {
        char categories[0x200];
        std::memset(categories, 0, sizeof(categories));

        ConsoleWrite("Console help categories: ", WARNING_COLOR);

        const size_t categoryCount = sizeof(kHelpCategories) / sizeof(kHelpCategories[0]);
        for (size_t i = 0; i < categoryCount; i++) {
            AppendText(categories, sizeof(categories), kHelpCategories[i].name);

            if (i + 1 < categoryCount) {
                AppendText(categories, sizeof(categories), ", ");
            }
        }

        ConsoleWrite(categories, WARNING_COLOR);
        ConsoleWrite("For more information type 'help [command] or [category]'", WARNING_COLOR);

        return 1;
    }

    const HelpCategory* matched = nullptr;
    for (const auto& category : kHelpCategories) {
        if (SStrCmpI(category.name, args, STORM_MAX_STR) == 0) {
            matched = &category;
            break;
        }
    }

    if (matched && matched->category != NONE) {
        char line[0x200];
        std::memset(line, 0, sizeof(line));

        char header[0x200];
        SStrPrintf(header, sizeof(header), "Commands registered for the category %s:", matched->name);
        ConsoleWrite(header, WARNING_COLOR);

        uint32_t count = 0;
        for (auto commandPtr = g_consoleCommandHash.Head(); commandPtr; commandPtr = g_consoleCommandHash.Next(commandPtr)) {
            if (commandPtr->category != matched->category) {
                continue;
            }

            AppendText(line, sizeof(line), commandPtr->command);
            AppendText(line, sizeof(line), ", ");

            count++;
            if (count == 8) {
                ConsoleWrite(line, WARNING_COLOR);
                line[0] = '\0';
                count = 0;
            }
        }

        if (line[0] == '\0') {
            ConsoleWrite("NONE", WARNING_COLOR);
        } else {
            char* comma = SStrChrR(line, ',');
            if (comma) {
                *comma = '\0';
            }

            ConsoleWrite(line, WARNING_COLOR);
        }

        return 1;
    }

    auto commandPtr = g_consoleCommandHash.Ptr(args);
    if (commandPtr) {
        char message[0xC8];

        SStrPrintf(message, 0xC5, "Help for command %s:", args);
        ConsoleWrite(message, WARNING_COLOR);

        const char* helpText = commandPtr->helpText ? commandPtr->helpText : "No help yet";
        SStrPrintf(message, 0xC5, "     %s %s", args, helpText);
        ConsoleWrite(message, WARNING_COLOR);
    }

    return 1;
}

int32_t ConsoleCommand_Quit(const char* command, const char* arguments) {
    // TODO
    // ConsolePostClose()

    return 0;
}

int32_t ConsoleCommand_SetMap(const char* command, const char* arguments) {
    return 1;
}

int32_t ConsoleCommand_Ver(const char* command, const char* arguments) {
    // TODO

    return 0;
}
