cmake_minimum_required(VERSION 3.16)

project(SCAPMS VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(SCAPMS
    main.cpp
    gui/MainWindow.cpp
    gui/MainWindow.h
    gui/PatientPanel.cpp
    gui/PatientPanel.h
    gui/AppointmentPanel.cpp
    gui/AppointmentPanel.h
    gui/QueuePanel.cpp
    gui/QueuePanel.h
    gui/SearchPanel.cpp
    gui/SearchPanel.h
    gui/ReportsPanel.cpp
    gui/ReportsPanel.h
    model/PatientRecord.cpp
    model/GeneralPatient.cpp
    model/EmergencyPatient.cpp
    model/FollowUpPatient.cpp
    model/Appointment.cpp
    model/ClinicDatabase.cpp
    utils/FileHandler.cpp
    controller/SearchEngine.cpp
    controller/ClinicManager.cpp
    controller/AppointmentController.cpp
    controller/Reports.cpp
    model/Appointable.h
    model/PatientRecord.h
    model/GeneralPatient.h
    model/EmergencyPatient.h
    model/FollowUpPatient.h
    model/Appointment.h
    model/ClinicDatabase.h
    utils/ClinicException.h
    utils/IDGenerator.h
    utils/Validator.h
    utils/FileHandler.h
    utils/Sorting.h
    controller/SearchEngine.h
    controller/ClinicManager.h
    controller/AppointmentController.h
    controller/Reports.h
)

target_link_libraries(SCAPMS PRIVATE Qt6::Widgets)
