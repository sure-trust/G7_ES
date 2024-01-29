#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#define SPIFFS_PARTITION_LABEL "storage"
#define MAX_ROWS 10
#define MAX_COLS 5
#define MAX_FIELD_SIZE 20
#define MAX_FILENAME_SIZE 30

static const char *TAG = "SPIFFS_DATABASE";

SemaphoreHandle_t spiffs_mutex;
int num_employees = 0; // Track the number of employees

typedef struct {
    char Employee_Name[MAX_FIELD_SIZE];
    char Employee_ID[MAX_FIELD_SIZE];
    char Employee_Age[MAX_FIELD_SIZE];
    char Employee_Position[MAX_FIELD_SIZE];
    char Employee_Salary[MAX_FIELD_SIZE];
} EmployeeDetails;

void initialize_spiffs() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = SPIFFS_PARTITION_LABEL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS initialized successfully");
    }
}

void lock_spiffs() {
    xSemaphoreTake(spiffs_mutex, portMAX_DELAY);
}

void unlock_spiffs() {
    xSemaphoreGive(spiffs_mutex);
}

void write_employee_details(int employee_index, const EmployeeDetails *employee) {
    lock_spiffs();

    char filename[MAX_FILENAME_SIZE];
    snprintf(filename, sizeof(filename), "/spiffs/data_%d.txt", employee_index);

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        unlock_spiffs();
        return;
    }

    fprintf(file, "%s\n%s\n%s\n%s\n%s\n",
            employee->Employee_Name, employee->Employee_ID, employee->Employee_Age,
            employee->Employee_Position, employee->Employee_Salary);
    fclose(file);

    unlock_spiffs();
}


void read_employee_details(int employee_index, EmployeeDetails *employee) {
    lock_spiffs();

    char filename[MAX_FILENAME_SIZE];
    snprintf(filename, sizeof(filename), "/spiffs/data_%d.txt", employee_index);

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", filename);
        unlock_spiffs();
        return;
    }

    // Read each line and assign to the corresponding field
    if (fscanf(file, "%19[^\n]%*c", employee->Employee_Name) != 1) {
        ESP_LOGE(TAG, "Failed to read Employee_Name from file: %s", filename);
        memset(employee, 0, sizeof(EmployeeDetails));
    } else if (fscanf(file, "%19[^\n]%*c", employee->Employee_ID) != 1) {
        ESP_LOGE(TAG, "Failed to read Employee_ID from file: %s", filename);
        memset(employee, 0, sizeof(EmployeeDetails));
    } else if (fscanf(file, "%19[^\n]%*c", employee->Employee_Age) != 1) {
        ESP_LOGE(TAG, "Failed to read Employee_Age from file: %s", filename);
        memset(employee, 0, sizeof(EmployeeDetails));
    } else if (fscanf(file, "%19[^\n]%*c", employee->Employee_Position) != 1) {
        ESP_LOGE(TAG, "Failed to read Employee_Position from file: %s", filename);
        memset(employee, 0, sizeof(EmployeeDetails));
    } else if (fscanf(file, "%19[^\n]%*c", employee->Employee_Salary) != 1) {
        ESP_LOGE(TAG, "Failed to read Employee_Salary from file: %s", filename);
        memset(employee, 0, sizeof(EmployeeDetails));
    }

    fclose(file);
    unlock_spiffs();
}

void get_employee_details_by_id(const char *employee_id, EmployeeDetails *employee) {
    // Assuming Employee_ID is unique, iterate through all employees to find the matching ID
    for (int i = 0; i < MAX_ROWS; i++) {
        EmployeeDetails current_employee;
        read_employee_details(i, &current_employee);

        if (strcmp(current_employee.Employee_ID, employee_id) == 0) {
            // Match found, copy details to the output parameter
            memcpy(employee, &current_employee, sizeof(EmployeeDetails));
            return;
        }
    }

    // If the function reaches here, no matching ID was found
    memset(employee, 0, sizeof(EmployeeDetails));
}

void add_new_employee(const char *name, const char *id, const char *age, const char *position, const char *salary) {
    // Check if there is space to add a new employee
    if (num_employees >= MAX_ROWS) {
        ESP_LOGE(TAG, "Cannot add new employee. Database is full.");
        return;
    }

    // Create a new employee with the provided details
    EmployeeDetails new_employee;
    snprintf(new_employee.Employee_Name, sizeof(new_employee.Employee_Name), "%s", name);
    snprintf(new_employee.Employee_ID, sizeof(new_employee.Employee_ID), "%s", id);
    snprintf(new_employee.Employee_Age, sizeof(new_employee.Employee_Age), "%s", age);
    snprintf(new_employee.Employee_Position, sizeof(new_employee.Employee_Position), "%s", position);
    snprintf(new_employee.Employee_Salary, sizeof(new_employee.Employee_Salary), "%s", salary);

    // Write the details of the new employee to SPIFFS
    write_employee_details(num_employees, &new_employee);

    // Increment the number of employees
    num_employees++;

    // Print details of the new employee
    ESP_LOGI(TAG, "New Employee Added:");
    ESP_LOGI(TAG, "Name: %s", new_employee.Employee_Name);
    ESP_LOGI(TAG, "ID: %s", new_employee.Employee_ID);
    ESP_LOGI(TAG, "Age: %s", new_employee.Employee_Age);
    ESP_LOGI(TAG, "Position: %s", new_employee.Employee_Position);
    ESP_LOGI(TAG, "Salary: %s", new_employee.Employee_Salary);
}

void delete_employee_by_id(const char *employee_id) {
    // Search for the employee with the given ID and delete their details
    for (int i = 0; i < MAX_ROWS; i++) {
        EmployeeDetails current_employee;
        read_employee_details(i, &current_employee);

        if (strcmp(current_employee.Employee_ID, employee_id) == 0) {
            // Employee found, clear their details from SPIFFS
            memset(&current_employee, 0, sizeof(EmployeeDetails));
            write_employee_details(i, &current_employee);
            ESP_LOGI(TAG, "Employee with ID %s deleted successfully.", employee_id);
            return;
        }
    }

    // If the function reaches here, no matching ID was found
    ESP_LOGE(TAG, "Employee with ID %s not found.", employee_id);
}

void app_main() {
    spiffs_mutex = xSemaphoreCreateMutex();

    initialize_spiffs();

    EmployeeDetails employees[MAX_ROWS] = {
        {"JohnDoe", "EMP001", "30", "SeniorEngineer", "50000"},
        {"JaneSmith", "EMP002", "35", "Manager", "70000"},
        {"Ravi", "EMP003", "28", "SeniorExecutive", "40000"},
        {"Raju", "EMP004", "29", "JuniorExecutive", "30000"},
        {"Vivek", "EMP005", "21", "CEO", "100000"},
        {"Monoj", "EMP006", "25", "MarketingExecutive", "40000"},
        {"Harish", "EMP007", "25", "JuniorEngineer", "40000"},
        {"Ankit", "EMP008", "26", "SalesExecutive", "30000"},
        {"Sai", "EMP009", "26", "Developer", "25000"},
        {"Eswar", "EMP010", "28", "TeamLead", "40000"}
    };

    // Write employee details to SPIFFS
    for (int i = 0; i < MAX_ROWS; i++) {
        write_employee_details(i, &employees[i]);
    }

    // Add more employees using the add_new_employee function
    add_new_employee("New Employee 1", "EMP011", "25", "New Position 1", "60000");
    add_new_employee("New Employee 2", "EMP012", "27", "New Position 2", "65000");
    add_new_employee("New Employee 3", "EMP013", "29", "New Position 3", "70000");
    add_new_employee("New Employee 4", "EMP014", "30", "New Position 4", "75000");

    // Delete an employee by their ID
   delete_employee_by_id("EMP006");

    // Retrieve details of an employee by ID
    EmployeeDetails target_employee;
    const char *target_employee_id = "EMP009";
    get_employee_details_by_id(target_employee_id, &target_employee);

    // Print details of the employee
    ESP_LOGI(TAG, "Employee Details for ID %s:", target_employee_id);
    ESP_LOGI(TAG, "Name: %s", target_employee.Employee_Name);
    ESP_LOGI(TAG, "Age: %s", target_employee.Employee_Age);
    ESP_LOGI(TAG, "Position: %s", target_employee.Employee_Position);
    ESP_LOGI(TAG, "Salary: %s", target_employee.Employee_Salary);
}
