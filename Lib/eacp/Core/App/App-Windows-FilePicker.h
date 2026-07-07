#pragma once

// Windows-only seam behind chooseFile() / chooseDirectory().
//
// The native picker is an interactive modal (IFileOpenDialog::Show), so it
// cannot be driven from a headless unit test. Everything around the modal —
// turning the picked wide path into the UTF-8 result, telling cancel from a
// selection, and building the file-type filter — is pulled out here as pure
// functions the tests exercise directly. App-Windows.cpp wires these to the
// real dialog; the tests wire chooseWithDialog() to a fake one.

#include "App.h"

#include <functional>
#include <optional>
#include <string>

namespace eacp::Apps::detail
{
// {"wav", "mp3"} -> L"*.wav;*.mp3". Empty in -> empty out (caller applies no
// filter, i.e. every file is shown).
std::wstring buildFilterPattern(const Vector<std::string>& extensions);

// The wide path the shell dialog handed back -> the UTF-8 string the public
// API returns. nullptr or empty (the user cancelled) -> nullopt.
std::optional<std::string> shellResultToPath(const wchar_t* pickedWidePath);

// Shows a native "open" dialog and yields the selected path, or nullopt on
// cancel; `pickFolders` chooses folder vs. file selection. This is the one
// piece that blocks on a human, so it is the injection point — the real
// implementation lives in App-Windows.cpp; tests pass a fake.
using ShellOpenDialog = std::function<std::optional<std::wstring>(
    bool pickFolders, const FilePickerOptions& options)>;

// Shared core behind chooseFile() / chooseDirectory(): run `dialog`, then
// convert its result to the public UTF-8 form. Split out so a fake `dialog`
// can prove the picked path is honoured without showing a modal — the bug was
// that the Windows stubs returned nullopt regardless of what the user chose.
std::optional<std::string> chooseWithDialog(bool pickFolders,
                                            const FilePickerOptions& options,
                                            const ShellOpenDialog& dialog);
} // namespace eacp::Apps::detail
