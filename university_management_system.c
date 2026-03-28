/*
 * University Academic & Research Management System (UARMS)
 * Console-based system written in ANSI C using structured programming
 * and binary file persistence.
 *
 * Compile: gcc university_management_system.c -o university -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* POSIX termios used only for password masking; falls back gracefully */
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
#  include <termios.h>
#  define HAVE_TERMIOS 1
#endif

/* ─── File names ──────────────────────────────────────────────────────────── */
#define USERS_FILE       "university_users.dat"
#define STUDENTS_FILE    "students.dat"
#define FACULTY_FILE     "faculty.dat"
#define COURSES_FILE     "courses.dat"
#define ENROLLMENTS_FILE "enrollments.dat"
#define RESEARCH_FILE    "research.dat"
#define LOG_FILE         "university_logs.txt"

/* ─── Field sizes ─────────────────────────────────────────────────────────── */
#define ID_LEN     20
#define NAME_LEN   60
#define DEPT_LEN   60
#define DOMAIN_LEN 60
#define PASS_LEN   64
#define TITLE_LEN  100
#define ROLE_LEN   20

/* ─── Grade distribution thresholds (grade-point scale 0.0 – 4.0+) ──────── */
#define GRADE_A_MIN 4.0f
#define GRADE_B_MIN 3.0f
#define GRADE_C_MIN 2.0f
#define GRADE_D_MIN 1.0f

/* ─── Structs ─────────────────────────────────────────────────────────────── */
typedef struct {
    char username[NAME_LEN];
    unsigned long password_hash;
    char role[ROLE_LEN];
} User;

typedef struct {
    char student_id[ID_LEN];
    char name[NAME_LEN];
    char department[DEPT_LEN];
    float cgpa;
} Student;

typedef struct {
    char faculty_id[ID_LEN];
    char name[NAME_LEN];
    char department[DEPT_LEN];
    double salary;
} Faculty;

typedef struct {
    char course_id[ID_LEN];
    char course_name[NAME_LEN];
    int credit;
} Course;

typedef struct {
    char student_id[ID_LEN];
    char course_id[ID_LEN];
    float grade;
} Enrollment;

typedef struct {
    char research_id[ID_LEN];
    char title[TITLE_LEN];
    char researcher[NAME_LEN];
    char domain[DOMAIN_LEN];
} Research;

/* ─── Global session ──────────────────────────────────────────────────────── */
static char g_current_user[NAME_LEN] = "";

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility helpers
 * ═════════════════════════════════════════════════════════════════════════ */

/* Read a line from stdin, stripping the trailing newline */
static void read_line(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buf, size, stdin) != NULL) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        }
    }
}

/* Read a password from stdin without echoing characters (POSIX only).
 * Falls back to plain fgets on platforms without termios. */
static void read_password(const char *prompt, char *buf, int size) {
#ifdef HAVE_TERMIOS
    struct termios old_term, new_term;
    int fd = fileno(stdin);
    int restored = 0;
    printf("%s", prompt);
    fflush(stdout);
    if (tcgetattr(fd, &old_term) == 0) {
        new_term = old_term;
        new_term.c_lflag &= (tcflag_t)~(ECHO | ECHOE | ECHOK | ECHONL);
        if (tcsetattr(fd, TCSANOW, &new_term) == 0) {
            restored = 1;
        }
    }
    if (fgets(buf, size, stdin) != NULL) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        }
    }
    if (restored) {
        tcsetattr(fd, TCSANOW, &old_term); /* Restore echo */
    }
    printf("\n"); /* Advance line since echo was off */
#else
    read_line(prompt, buf, size);
#endif
}

/* djb2 hash function (Dan Bernstein).
 * NOTE: djb2 is used as required by the project specification. It is not a
 * cryptographic hash; production systems should use bcrypt/PBKDF2/scrypt. */
static unsigned long djb2_hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++) != 0) {
        hash = ((hash << 5) + hash) + (unsigned long)c;
    }
    return hash;
}

/* ─── Audit logging ───────────────────────────────────────────────────────── */
static void audit_log(const char *event) {
    FILE *fp;
    time_t now;
    struct tm *t;
    char timestamp[32];

    fp = fopen(LOG_FILE, "a");
    if (fp == NULL) {
        return;
    }
    now = time(NULL);
    t = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    fprintf(fp, "[%s] USER=%s | %s\n", timestamp, g_current_user, event);
    fclose(fp);
}

/* ─── Press-enter pause ───────────────────────────────────────────────────── */
static void press_enter(void) {
    printf("\nPress ENTER to continue...");
    fflush(stdout);
    while (getchar() != '\n')
        ;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Authentication Module
 * ═════════════════════════════════════════════════════════════════════════ */

/* Seed the default admin account if the users file is empty / absent */
static void seed_default_admin(void) {
    FILE *fp;
    User u;
    int count = 0;

    fp = fopen(USERS_FILE, "rb");
    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        count = (int)(ftell(fp) / (long)sizeof(User));
        fclose(fp);
    }
    if (count > 0) {
        return;
    }

    /* Create default admin: username=admin password=admin123 */
    strncpy(u.username, "admin", NAME_LEN - 1);
    u.username[NAME_LEN - 1] = '\0';
    u.password_hash = djb2_hash("admin123");
    strncpy(u.role, "admin", ROLE_LEN - 1);
    u.role[ROLE_LEN - 1] = '\0';

    fp = fopen(USERS_FILE, "wb");
    if (fp == NULL) {
        perror("Cannot create users file");
        return;
    }
    fwrite(&u, sizeof(User), 1, fp);
    fclose(fp);
}

/* Returns 1 on successful login, 0 on failure */
static int login(void) {
    char username[NAME_LEN];
    char password[PASS_LEN];
    unsigned long entered_hash;
    FILE *fp;
    User u;
    int attempts;

    seed_default_admin();

    for (attempts = 0; attempts < 3; attempts++) {
        printf("\n╔══════════════════════════════╗\n");
        printf("║        UARMS  LOGIN          ║\n");
        printf("╚══════════════════════════════╝\n");
        read_line("  Username : ", username, NAME_LEN);
        read_password("  Password : ", password, PASS_LEN);

        entered_hash = djb2_hash(password);

        fp = fopen(USERS_FILE, "rb");
        if (fp == NULL) {
            printf("  [ERROR] Cannot open users file.\n");
            return 0;
        }
        while (fread(&u, sizeof(User), 1, fp) == 1) {
            if (strcmp(u.username, username) == 0 &&
                u.password_hash == entered_hash) {
                fclose(fp);
                strncpy(g_current_user, username, NAME_LEN - 1);
                g_current_user[NAME_LEN - 1] = '\0';
                audit_log("LOGIN_SUCCESS");
                printf("\n  Welcome, %s! (Role: %s)\n", username, u.role);
                return 1;
            }
        }
        fclose(fp);
        printf("  [ERROR] Invalid credentials. %d attempt(s) left.\n",
               2 - attempts);
        audit_log("LOGIN_FAILED");
    }
    return 0;
}

/* Add a new user account (admin function) */
static void add_user(void) {
    User u;
    char password[PASS_LEN];
    FILE *fp;

    printf("\n── Add New User ──\n");
    read_line("  Username : ", u.username, NAME_LEN);
    read_password("  Password : ", password, PASS_LEN);
    read_line("  Role     : ", u.role, ROLE_LEN);
    u.password_hash = djb2_hash(password);

    fp = fopen(USERS_FILE, "ab");
    if (fp == NULL) {
        perror("Cannot open users file");
        return;
    }
    fwrite(&u, sizeof(User), 1, fp);
    fclose(fp);
    audit_log("ADD_USER");
    printf("  [OK] User '%s' added.\n", u.username);
}

/* List all users */
static void view_users(void) {
    FILE *fp;
    User u;
    int n = 0;

    printf("\n── User Accounts ──\n");
    printf("  %-20s %-12s\n", "Username", "Role");
    printf("  %-20s %-12s\n", "--------", "----");

    fp = fopen(USERS_FILE, "rb");
    if (fp == NULL) {
        printf("  (no records)\n");
        return;
    }
    while (fread(&u, sizeof(User), 1, fp) == 1) {
        printf("  %-20s %-12s\n", u.username, u.role);
        n++;
    }
    fclose(fp);
    if (n == 0) {
        printf("  (no records)\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Student Management
 * ═════════════════════════════════════════════════════════════════════════ */

static void add_student(void) {
    Student s;
    char buf[32];
    FILE *fp;

    printf("\n── Add Student ──\n");
    read_line("  Student ID  : ", s.student_id, ID_LEN);
    read_line("  Name        : ", s.name, NAME_LEN);
    read_line("  Department  : ", s.department, DEPT_LEN);
    read_line("  CGPA        : ", buf, sizeof(buf));
    s.cgpa = (float)atof(buf);

    fp = fopen(STUDENTS_FILE, "ab");
    if (fp == NULL) {
        perror("Cannot open students file");
        return;
    }
    fwrite(&s, sizeof(Student), 1, fp);
    fclose(fp);
    audit_log("ADD_STUDENT");
    printf("  [OK] Student '%s' added.\n", s.name);
}

static void view_students(void) {
    FILE *fp;
    Student s;
    int n = 0;

    printf("\n── Student Records ──\n");
    printf("  %-12s %-25s %-20s %s\n",
           "Student ID", "Name", "Department", "CGPA");
    printf("  %-12s %-25s %-20s %s\n",
           "----------", "----", "----------", "----");

    fp = fopen(STUDENTS_FILE, "rb");
    if (fp == NULL) {
        printf("  (no records)\n");
        return;
    }
    while (fread(&s, sizeof(Student), 1, fp) == 1) {
        printf("  %-12s %-25s %-20s %.2f\n",
               s.student_id, s.name, s.department, s.cgpa);
        n++;
    }
    fclose(fp);
    if (n == 0) {
        printf("  (no records)\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Faculty Management
 * ═════════════════════════════════════════════════════════════════════════ */

static void add_faculty(void) {
    Faculty f;
    char buf[32];
    FILE *fp;

    printf("\n── Add Faculty ──\n");
    read_line("  Faculty ID  : ", f.faculty_id, ID_LEN);
    read_line("  Name        : ", f.name, NAME_LEN);
    read_line("  Department  : ", f.department, DEPT_LEN);
    read_line("  Salary      : ", buf, sizeof(buf));
    f.salary = atof(buf);

    fp = fopen(FACULTY_FILE, "ab");
    if (fp == NULL) {
        perror("Cannot open faculty file");
        return;
    }
    fwrite(&f, sizeof(Faculty), 1, fp);
    fclose(fp);
    audit_log("ADD_FACULTY");
    printf("  [OK] Faculty '%s' added.\n", f.name);
}

static void view_faculty(void) {
    FILE *fp;
    Faculty f;
    int n = 0;

    printf("\n── Faculty Records ──\n");
    printf("  %-12s %-25s %-20s %s\n",
           "Faculty ID", "Name", "Department", "Salary");
    printf("  %-12s %-25s %-20s %s\n",
           "----------", "----", "----------", "------");

    fp = fopen(FACULTY_FILE, "rb");
    if (fp == NULL) {
        printf("  (no records)\n");
        return;
    }
    while (fread(&f, sizeof(Faculty), 1, fp) == 1) {
        printf("  %-12s %-25s %-20s %.2f\n",
               f.faculty_id, f.name, f.department, f.salary);
        n++;
    }
    fclose(fp);
    if (n == 0) {
        printf("  (no records)\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Course Management
 * ═════════════════════════════════════════════════════════════════════════ */

static void add_course(void) {
    Course c;
    char buf[32];
    FILE *fp;

    printf("\n── Add Course ──\n");
    read_line("  Course ID   : ", c.course_id, ID_LEN);
    read_line("  Course Name : ", c.course_name, NAME_LEN);
    read_line("  Credits     : ", buf, sizeof(buf));
    c.credit = atoi(buf);

    fp = fopen(COURSES_FILE, "ab");
    if (fp == NULL) {
        perror("Cannot open courses file");
        return;
    }
    fwrite(&c, sizeof(Course), 1, fp);
    fclose(fp);
    audit_log("ADD_COURSE");
    printf("  [OK] Course '%s' added.\n", c.course_name);
}

static void view_courses(void) {
    FILE *fp;
    Course c;
    int n = 0;

    printf("\n── Course Records ──\n");
    printf("  %-12s %-30s %s\n", "Course ID", "Course Name", "Credits");
    printf("  %-12s %-30s %s\n", "---------", "-----------", "-------");

    fp = fopen(COURSES_FILE, "rb");
    if (fp == NULL) {
        printf("  (no records)\n");
        return;
    }
    while (fread(&c, sizeof(Course), 1, fp) == 1) {
        printf("  %-12s %-30s %d\n", c.course_id, c.course_name, c.credit);
        n++;
    }
    fclose(fp);
    if (n == 0) {
        printf("  (no records)\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Enrollment System
 * ═════════════════════════════════════════════════════════════════════════ */

static void enroll_student(void) {
    Enrollment e;
    char buf[32];
    FILE *fp;

    printf("\n── Enroll Student in Course ──\n");
    read_line("  Student ID : ", e.student_id, ID_LEN);
    read_line("  Course ID  : ", e.course_id, ID_LEN);
    read_line("  Grade      : ", buf, sizeof(buf));
    e.grade = (float)atof(buf);

    fp = fopen(ENROLLMENTS_FILE, "ab");
    if (fp == NULL) {
        perror("Cannot open enrollments file");
        return;
    }
    fwrite(&e, sizeof(Enrollment), 1, fp);
    fclose(fp);
    audit_log("ENROLL_STUDENT");
    printf("  [OK] Student '%s' enrolled in course '%s'.\n",
           e.student_id, e.course_id);
}

static void view_enrollments(void) {
    FILE *fp;
    Enrollment e;
    int n = 0;

    printf("\n── Enrollment Records ──\n");
    printf("  %-12s %-12s %s\n", "Student ID", "Course ID", "Grade");
    printf("  %-12s %-12s %s\n", "----------", "---------", "-----");

    fp = fopen(ENROLLMENTS_FILE, "rb");
    if (fp == NULL) {
        printf("  (no records)\n");
        return;
    }
    while (fread(&e, sizeof(Enrollment), 1, fp) == 1) {
        printf("  %-12s %-12s %.2f\n", e.student_id, e.course_id, e.grade);
        n++;
    }
    fclose(fp);
    if (n == 0) {
        printf("  (no records)\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Research Management
 * ═════════════════════════════════════════════════════════════════════════ */

static void add_research(void) {
    Research r;
    FILE *fp;

    printf("\n── Add Research Record ──\n");
    read_line("  Research ID : ", r.research_id, ID_LEN);
    read_line("  Title       : ", r.title, TITLE_LEN);
    read_line("  Researcher  : ", r.researcher, NAME_LEN);
    read_line("  Domain      : ", r.domain, DOMAIN_LEN);

    fp = fopen(RESEARCH_FILE, "ab");
    if (fp == NULL) {
        perror("Cannot open research file");
        return;
    }
    fwrite(&r, sizeof(Research), 1, fp);
    fclose(fp);
    audit_log("ADD_RESEARCH");
    printf("  [OK] Research '%s' added.\n", r.title);
}

static void view_research(void) {
    FILE *fp;
    Research r;
    int n = 0;

    printf("\n── Research Records ──\n");
    printf("  %-12s %-35s %-20s %s\n",
           "Research ID", "Title", "Researcher", "Domain");
    printf("  %-12s %-35s %-20s %s\n",
           "-----------", "-----", "----------", "------");

    fp = fopen(RESEARCH_FILE, "rb");
    if (fp == NULL) {
        printf("  (no records)\n");
        return;
    }
    while (fread(&r, sizeof(Research), 1, fp) == 1) {
        printf("  %-12s %-35s %-20s %s\n",
               r.research_id, r.title, r.researcher, r.domain);
        n++;
    }
    fclose(fp);
    if (n == 0) {
        printf("  (no records)\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Grade Analytics
 * ═════════════════════════════════════════════════════════════════════════ */

static void grade_analytics(void) {
    FILE *fp;
    Enrollment e;
    double sum = 0.0, sum_sq = 0.0, mean, variance, std_dev;
    int n = 0;
    int dist[5]; /* A:4-5, B:3-4, C:2-3, D:1-2, F:0-1 */
    int i;

    memset(dist, 0, sizeof(dist));

    fp = fopen(ENROLLMENTS_FILE, "rb");
    if (fp == NULL) {
        printf("\n  [INFO] No enrollment data available.\n");
        return;
    }
    while (fread(&e, sizeof(Enrollment), 1, fp) == 1) {
        sum    += e.grade;
        sum_sq += (double)e.grade * (double)e.grade;
        n++;

        /* Grade distribution buckets (grade points 0.0 – 4.0+) */
        if (e.grade >= GRADE_A_MIN)                              dist[0]++;
        else if (e.grade >= GRADE_B_MIN && e.grade < GRADE_A_MIN) dist[1]++;
        else if (e.grade >= GRADE_C_MIN && e.grade < GRADE_B_MIN) dist[2]++;
        else if (e.grade >= GRADE_D_MIN && e.grade < GRADE_C_MIN) dist[3]++;
        else                                                       dist[4]++;
    }
    fclose(fp);

    if (n == 0) {
        printf("\n  [INFO] No enrollment records found.\n");
        return;
    }

    mean     = sum / n;
    variance = (sum_sq / n) - (mean * mean);
    if (variance < 0.0) variance = 0.0; /* guard against negative variance from floating-point precision errors */
    std_dev  = sqrt(variance);

    printf("\n── Grade Analytics ──\n");
    printf("  Total Enrollments : %d\n", n);
    printf("  Mean Grade        : %.4f\n", mean);
    printf("  Variance          : %.4f\n", variance);
    printf("  Std Deviation     : %.4f\n", std_dev);
    printf("\n  Grade Distribution:\n");

    printf("  %-8s %-8s %s\n", "Grade", "Count", "Bar");
    printf("  %-8s %-8s %s\n", "-----", "-----", "---");

    {
        const char *labels[] = {"A (4+)", "B (3-4)", "C (2-3)", "D (1-2)", "F (<1)"};
        for (i = 0; i < 5; i++) {
            int bar;
            printf("  %-8s %-8d ", labels[i], dist[i]);
            for (bar = 0; bar < dist[i]; bar++) printf("█");
            printf("\n");
        }
    }

    audit_log("GRADE_ANALYTICS");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * View Audit Log
 * ═════════════════════════════════════════════════════════════════════════ */

static void view_audit_log(void) {
    FILE *fp;
    char line[256];
    int n = 0;

    printf("\n── Audit Log ──\n");
    fp = fopen(LOG_FILE, "r");
    if (fp == NULL) {
        printf("  (no log entries)\n");
        return;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        printf("  %s", line);
        n++;
    }
    fclose(fp);
    if (n == 0) {
        printf("  (no log entries)\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Sub-menus
 * ═════════════════════════════════════════════════════════════════════════ */

static void menu_students(void) {
    char choice[8];
    int running = 1;
    while (running) {
        printf("\n╔══════════════════════════════╗\n");
        printf("║    Student Management        ║\n");
        printf("╠══════════════════════════════╣\n");
        printf("║  1. Add Student              ║\n");
        printf("║  2. View Students            ║\n");
        printf("║  0. Back                     ║\n");
        printf("╚══════════════════════════════╝\n");
        read_line("  Choice: ", choice, sizeof(choice));
        switch (choice[0]) {
            case '1': add_student();   break;
            case '2': view_students(); break;
            case '0': running = 0;     break;
            default: printf("  [WARN] Invalid choice.\n"); break;
        }
        if (running) press_enter();
    }
}

static void menu_faculty(void) {
    char choice[8];
    int running = 1;
    while (running) {
        printf("\n╔══════════════════════════════╗\n");
        printf("║    Faculty Management        ║\n");
        printf("╠══════════════════════════════╣\n");
        printf("║  1. Add Faculty              ║\n");
        printf("║  2. View Faculty             ║\n");
        printf("║  0. Back                     ║\n");
        printf("╚══════════════════════════════╝\n");
        read_line("  Choice: ", choice, sizeof(choice));
        switch (choice[0]) {
            case '1': add_faculty();   break;
            case '2': view_faculty();  break;
            case '0': running = 0;     break;
            default: printf("  [WARN] Invalid choice.\n"); break;
        }
        if (running) press_enter();
    }
}

static void menu_courses(void) {
    char choice[8];
    int running = 1;
    while (running) {
        printf("\n╔══════════════════════════════╗\n");
        printf("║    Course Management         ║\n");
        printf("╠══════════════════════════════╣\n");
        printf("║  1. Add Course               ║\n");
        printf("║  2. View Courses             ║\n");
        printf("║  0. Back                     ║\n");
        printf("╚══════════════════════════════╝\n");
        read_line("  Choice: ", choice, sizeof(choice));
        switch (choice[0]) {
            case '1': add_course();   break;
            case '2': view_courses(); break;
            case '0': running = 0;    break;
            default: printf("  [WARN] Invalid choice.\n"); break;
        }
        if (running) press_enter();
    }
}

static void menu_enrollments(void) {
    char choice[8];
    int running = 1;
    while (running) {
        printf("\n╔══════════════════════════════╗\n");
        printf("║    Enrollment System         ║\n");
        printf("╠══════════════════════════════╣\n");
        printf("║  1. Enroll Student           ║\n");
        printf("║  2. View Enrollments         ║\n");
        printf("║  0. Back                     ║\n");
        printf("╚══════════════════════════════╝\n");
        read_line("  Choice: ", choice, sizeof(choice));
        switch (choice[0]) {
            case '1': enroll_student();    break;
            case '2': view_enrollments();  break;
            case '0': running = 0;         break;
            default: printf("  [WARN] Invalid choice.\n"); break;
        }
        if (running) press_enter();
    }
}

static void menu_research(void) {
    char choice[8];
    int running = 1;
    while (running) {
        printf("\n╔══════════════════════════════╗\n");
        printf("║    Research Management       ║\n");
        printf("╠══════════════════════════════╣\n");
        printf("║  1. Add Research             ║\n");
        printf("║  2. View Research            ║\n");
        printf("║  0. Back                     ║\n");
        printf("╚══════════════════════════════╝\n");
        read_line("  Choice: ", choice, sizeof(choice));
        switch (choice[0]) {
            case '1': add_research();  break;
            case '2': view_research(); break;
            case '0': running = 0;     break;
            default: printf("  [WARN] Invalid choice.\n"); break;
        }
        if (running) press_enter();
    }
}

static void menu_auth(void) {
    char choice[8];
    int running = 1;
    while (running) {
        printf("\n╔══════════════════════════════╗\n");
        printf("║    Authentication            ║\n");
        printf("╠══════════════════════════════╣\n");
        printf("║  1. Add User                 ║\n");
        printf("║  2. View Users               ║\n");
        printf("║  0. Back                     ║\n");
        printf("╚══════════════════════════════╝\n");
        read_line("  Choice: ", choice, sizeof(choice));
        switch (choice[0]) {
            case '1': add_user();   break;
            case '2': view_users(); break;
            case '0': running = 0;  break;
            default: printf("  [WARN] Invalid choice.\n"); break;
        }
        if (running) press_enter();
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main menu
 * ═════════════════════════════════════════════════════════════════════════ */

static void main_menu(void) {
    char choice[8];
    int running = 1;
    while (running) {
        printf("\n╔══════════════════════════════════╗\n");
        printf("║  UARMS – Main Menu               ║\n");
        printf("╠══════════════════════════════════╣\n");
        printf("║  1. Student Management           ║\n");
        printf("║  2. Faculty Management           ║\n");
        printf("║  3. Course Management            ║\n");
        printf("║  4. Enrollment System            ║\n");
        printf("║  5. Research Management          ║\n");
        printf("║  6. Grade Analytics              ║\n");
        printf("║  7. Authentication Settings      ║\n");
        printf("║  8. View Audit Log               ║\n");
        printf("║  0. Logout & Exit                ║\n");
        printf("╚══════════════════════════════════╝\n");
        read_line("  Choice: ", choice, sizeof(choice));
        switch (choice[0]) {
            case '1': menu_students();   break;
            case '2': menu_faculty();    break;
            case '3': menu_courses();    break;
            case '4': menu_enrollments();break;
            case '5': menu_research();   break;
            case '6':
                grade_analytics();
                press_enter();
                break;
            case '7': menu_auth();       break;
            case '8':
                view_audit_log();
                press_enter();
                break;
            case '0':
                audit_log("LOGOUT");
                printf("\n  Goodbye, %s!\n\n", g_current_user);
                running = 0;
                break;
            default:
                printf("  [WARN] Invalid choice.\n");
                press_enter();
                break;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Entry point
 * ═════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  University Academic & Research Management  ║\n");
    printf("║              System (UARMS)                 ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("  Default credentials: admin / admin123\n");

    if (!login()) {
        printf("\n  [ERROR] Too many failed login attempts. Exiting.\n\n");
        return EXIT_FAILURE;
    }

    main_menu();
    return EXIT_SUCCESS;
}
