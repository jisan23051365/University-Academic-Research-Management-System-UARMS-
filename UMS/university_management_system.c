#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* =========================================================
   UNIVERSITY ACADEMIC & RESEARCH MANAGEMENT SYSTEM
   ========================================================= */

/* ===================== STRUCTURES ===================== */

typedef struct {
    int id;
    char username[30];
    unsigned long password_hash;
} User;

typedef struct {
    int id;
    char name[50];
    char department[50];
    float cgpa;
} Student;

typedef struct {
    int id;
    char name[50];
    char department[50];
    float salary;
} Faculty;

typedef struct {
    int id;
    char course_name[50];
    int credit;
} Course;

typedef struct {
    int student_id;
    int course_id;
    float grade;
} Enrollment;

typedef struct {
    int id;
    char title[100];
    char researcher[50];
    char domain[50];
} Research;

/* ===================== UTILITIES ===================== */

unsigned long hashPassword(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

void logActivity(const char *msg) {
    FILE *fp = fopen("university_logs.txt", "a");
    if (!fp) return;
    time_t now = time(NULL);
    fprintf(fp, "%s - %s\n", ctime(&now), msg);
    fprintf(fp, "--------------------------------\n");
    fclose(fp);
}

/* ===================== AUTH ===================== */

void initializeAdmin() {
    FILE *fp = fopen("university_users.dat", "rb");
    if (fp) { fclose(fp); return; }

    fp = fopen("university_users.dat", "wb");
    User admin = {1, "admin", hashPassword("admin123")};
    fwrite(&admin, sizeof(User), 1, fp);
    fclose(fp);
}

int login() {
    char username[30], password[30];
    unsigned long hash;
    User u;

    printf("Username: ");
    scanf("%s", username);
    printf("Password: ");
    scanf("%s", password);

    hash = hashPassword(password);

    FILE *fp = fopen("university_users.dat", "rb");
    if (!fp) return 0;

    while (fread(&u, sizeof(User), 1, fp)) {
        if (strcmp(u.username, username) == 0 && u.password_hash == hash) {
            fclose(fp);
            logActivity("Admin login successful.");
            return 1;
        }
    }

    fclose(fp);
    printf("Invalid credentials.\n");
    return 0;
}

/* ===================== STUDENT ===================== */

void addStudent() {
    FILE *fp = fopen("students.dat", "ab");
    if (!fp) return;

    Student s;
    printf("Student ID: "); scanf("%d", &s.id);
    printf("Name: "); scanf(" %[^\n]", s.name);
    printf("Department: "); scanf(" %[^\n]", s.department);
    printf("CGPA: "); scanf("%f", &s.cgpa);

    fwrite(&s, sizeof(Student), 1, fp);
    fclose(fp);
    logActivity("Student added.");
}

void viewStudents() {
    FILE *fp = fopen("students.dat", "rb");
    if (!fp) return;

    Student s;
    printf("\n--- Student Records ---\n");
    while (fread(&s, sizeof(Student), 1, fp)) {
        printf("ID:%d | %s | Dept:%s | CGPA:%.2f\n",
               s.id, s.name, s.department, s.cgpa);
    }
    fclose(fp);
}

/* ===================== FACULTY ===================== */

void addFaculty() {
    FILE *fp = fopen("faculty.dat", "ab");
    if (!fp) return;

    Faculty f;
    printf("Faculty ID: "); scanf("%d", &f.id);
    printf("Name: "); scanf(" %[^\n]", f.name);
    printf("Department: "); scanf(" %[^\n]", f.department);
    printf("Salary: "); scanf("%f", &f.salary);

    fwrite(&f, sizeof(Faculty), 1, fp);
    fclose(fp);
    logActivity("Faculty added.");
}

void viewFaculty() {
    FILE *fp = fopen("faculty.dat", "rb");
    if (!fp) return;

    Faculty f;
    printf("\n--- Faculty Records ---\n");
    while (fread(&f, sizeof(Faculty), 1, fp)) {
        printf("ID:%d | %s | Dept:%s | Salary:%.2f\n",
               f.id, f.name, f.department, f.salary);
    }
    fclose(fp);
}

/* ===================== COURSE ===================== */

void addCourse() {
    FILE *fp = fopen("courses.dat", "ab");
    if (!fp) return;

    Course c;
    printf("Course ID: "); scanf("%d", &c.id);
    printf("Course Name: "); scanf(" %[^\n]", c.course_name);
    printf("Credit: "); scanf("%d", &c.credit);

    fwrite(&c, sizeof(Course), 1, fp);
    fclose(fp);
    logActivity("Course added.");
}

void viewCourses() {
    FILE *fp = fopen("courses.dat", "rb");
    if (!fp) return;

    Course c;
    printf("\n--- Course Records ---\n");
    while (fread(&c, sizeof(Course), 1, fp)) {
        printf("ID:%d | %s | Credit:%d\n",
               c.id, c.course_name, c.credit);
    }
    fclose(fp);
}

/* ===================== ENROLLMENT ===================== */

void enrollStudent() {
    FILE *fp = fopen("enrollments.dat", "ab");
    if (!fp) return;

    Enrollment e;
    printf("Student ID: "); scanf("%d", &e.student_id);
    printf("Course ID: "); scanf("%d", &e.course_id);
    printf("Grade: "); scanf("%f", &e.grade);

    fwrite(&e, sizeof(Enrollment), 1, fp);
    fclose(fp);
    logActivity("Student enrolled in course.");
}

/* ===================== RESEARCH ===================== */

void addResearch() {
    FILE *fp = fopen("research.dat", "ab");
    if (!fp) return;

    Research r;
    printf("Research ID: "); scanf("%d", &r.id);
    printf("Title: "); scanf(" %[^\n]", r.title);
    printf("Researcher: "); scanf(" %[^\n]", r.researcher);
    printf("Domain: "); scanf(" %[^\n]", r.domain);

    fwrite(&r, sizeof(Research), 1, fp);
    fclose(fp);
    logActivity("Research record added.");
}

/* ===================== ANALYTICS ===================== */

void gradeAnalytics() {
    FILE *fp = fopen("enrollments.dat", "rb");
    if (!fp) return;

    Enrollment e;
    float arr[500];
    int n = 0;

    while (fread(&e, sizeof(Enrollment), 1, fp) && n < 500) {
        arr[n++] = e.grade;
    }
    fclose(fp);

    if (n == 0) {
        printf("No enrollment data.\n");
        return;
    }

    float sum = 0;
    for (int i = 0; i < n; i++) sum += arr[i];
    float mean = sum / n;

    float variance = 0;
    for (int i = 0; i < n; i++)
        variance += pow(arr[i] - mean, 2);
    variance /= n;

    float std = sqrt(variance);

    printf("\n--- Grade Analytics ---\n");
    printf("Total Grades: %d\n", n);
    printf("Average Grade: %.2f\n", mean);
    printf("Variance: %.2f\n", variance);
    printf("Standard Deviation: %.2f\n", std);

    logActivity("Grade analytics generated.");
}

/* ===================== MAIN ===================== */

int main() {
    initializeAdmin();

    printf("=== UNIVERSITY MANAGEMENT SYSTEM ===\n");

    if (!login()) return 0;

    int choice;

    while (1) {
        printf("\n1.Add Student\n");
        printf("2.View Students\n");
        printf("3.Add Faculty\n");
        printf("4.View Faculty\n");
        printf("5.Add Course\n");
        printf("6.View Courses\n");
        printf("7.Enroll Student\n");
        printf("8.Add Research\n");
        printf("9.Grade Analytics\n");
        printf("10.Exit\n");
        printf("Choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1: addStudent(); break;
            case 2: viewStudents(); break;
            case 3: addFaculty(); break;
            case 4: viewFaculty(); break;
            case 5: addCourse(); break;
            case 6: viewCourses(); break;
            case 7: enrollStudent(); break;
            case 8: addResearch(); break;
            case 9: gradeAnalytics(); break;
            case 10:
                logActivity("System exited.");
                exit(0);
            default:
                printf("Invalid choice.\n");
        }
    }

    return 0;
}