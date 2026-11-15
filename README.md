# Smart Clinic Appointment & Patient Management System

SCAPMS is a desktop lab project written in C++17 and Qt Widgets. It stores
patient records, appointments, consultation notes, and the waiting queue in a
local JSON file.

## Build and run on Ubuntu

Install the compiler, CMake, and Qt 6 development headers. Then run the build
commands from the extracted `SCAPMS` folder.

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/scapms data/demo-clinic.json
```

Run `./build/scapms` for an empty clinic. From the app, choose **File → Load**
to open a saved clinic document. Use **File → Save as** to keep the sample
intact while practising the workflows.

## Build on Windows with Qt Creator

Install Qt 6 with a desktop C++ kit and its matching compiler. Open
`CMakeLists.txt` in Qt Creator, select the installed desktop kit, then use
**Build → Build Project** and **Run**. In the app, use **File → Load** to
open `data/demo-clinic.json` from the extracted project. The same CMake
project also supports Qt 5.15 with its Widgets development package.

## Walk through the app

1. In **Patient Records**, register a patient. Choose a category to see its
   visit field, then double-click the new row to edit it.
2. In **Appointments**, schedule a future slot. Select a pending row and
   use **Complete** to record an outcome or **Cancel** to release its slot.
3. In **Waiting Queue**, add two patients and use **Call next patient**.
   The earlier entry is called first.
4. In **Search & Sort**, try binary search for ID `1001`, recursive name
   search for `an`, and a category search for Emergency. Use **Reset** to
   show all records, then sort by name, ID, or age with either algorithm.
5. In **Reports**, choose **8 November 2026** for the sample daily summary
   in the UK time zone. The other three reports cover the whole file.
6. Use **Edit → Undo** or **Ctrl+Z** after an administrative change. Use
   **Ctrl+S** to save, **Ctrl+F** to find a patient, or **Ctrl+Q** to quit.

The sample data is fictional. Dates in the JSON are stored in UTC and
displayed in the workstation's local time zone. Undo history and the recent
patient cache belong to the active session. Keep the sample file for demos.
