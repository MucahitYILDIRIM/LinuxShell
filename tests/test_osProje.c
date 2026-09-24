/*
 * osProje.c icin unit testler (Unity framework).
 * osProje.c dogrudan include edilir; UNIT_TEST tanimli oldugu icin main derlenmez.
 */
#define UNIT_TEST
#include "../osProje.c"
#include "unity.h"

#include <errno.h>
#include <sys/stat.h>

/* ---------- yardimcilar ---------- */

static int savedStdout = -1;
static char capturePath[64];
static char captureBuf[4096];

/* stdout'u gecici dosyaya yonlendirir; fork edilen cocuklar da ayni dosyaya yazar */
static void captureStart(void)
{
        fflush(stdout);
        strcpy(capturePath, "/tmp/osproje_capXXXXXX");
        int fd = mkstemp(capturePath);
        savedStdout = dup(STDOUT_FILENO);
        dup2(fd, STDOUT_FILENO);
        close(fd);
}

static const char *captureStop(void)
{
        fflush(stdout);
        dup2(savedStdout, STDOUT_FILENO);
        close(savedStdout);
        savedStdout = -1;

        memset(captureBuf, 0, sizeof(captureBuf));
        FILE *f = fopen(capturePath, "r");
        if (f != NULL)
        {
                fread(captureBuf, 1, sizeof(captureBuf) - 1, f);
                fclose(f);
        }
        unlink(capturePath);
        return captureBuf;
}

static void writeFile(const char *path, const char *content)
{
        FILE *f = fopen(path, "w");
        fputs(content, f);
        fclose(f);
}

static const char *readFile(const char *path)
{
        static char buf[4096];
        memset(buf, 0, sizeof(buf));
        FILE *f = fopen(path, "r");
        if (f != NULL)
        {
                fread(buf, 1, sizeof(buf) - 1, f);
                fclose(f);
        }
        return buf;
}

static int noChildrenLeft(void)
{
        return waitpid(-1, NULL, WNOHANG) == -1 && errno == ECHILD;
}

static char tmpPath[64];

void setUp(void)
{
        strcpy(tmpPath, "/tmp/osproje_testXXXXXX");
        int fd = mkstemp(tmpPath);
        close(fd);
        unlink(tmpPath); // sadece benzersiz isim lazim
}

void tearDown(void)
{
        signal(SIGCHLD, SIG_DFL); // singleProccessingBg'nin kurdugu handler temizlendi
        while (waitpid(-1, NULL, 0) > 0) {}
        unlink(tmpPath);
}

/* ---------- isPipe ---------- */

void test_isPipe_returnsZeroWithoutPipe(void)
{
        char *args[] = {"ls", "-l", NULL};
        TEST_ASSERT_EQUAL_INT(0, isPipe(args));
}

void test_isPipe_returnsOneWithPipe(void)
{
        char *args[] = {"ls", "|", "wc", NULL};
        TEST_ASSERT_EQUAL_INT(1, isPipe(args));
}

void test_isPipe_emptyCommand(void)
{
        char *args[] = {NULL};
        TEST_ASSERT_EQUAL_INT(0, isPipe(args));
}

void test_isPipe_ignoresPipeInsideToken(void)
{
        char *args[] = {"a|b", NULL};
        TEST_ASSERT_EQUAL_INT(0, isPipe(args));
}

/* ---------- tokenizeLine ---------- */

void test_tokenizeLine_splitsOnSpacesTabsNewline(void)
{
        char line[] = "ls  -l\t/tmp\n";
        char *tokens[LIMIT];
        TEST_ASSERT_EQUAL_INT(3, tokenizeLine(line, tokens));
        TEST_ASSERT_EQUAL_STRING("ls", tokens[0]);
        TEST_ASSERT_EQUAL_STRING("-l", tokens[1]);
        TEST_ASSERT_EQUAL_STRING("/tmp", tokens[2]);
        TEST_ASSERT_NULL(tokens[3]);
}

void test_tokenizeLine_emptyLineReturnsZero(void)
{
        char line[] = "\n";
        char *tokens[LIMIT];
        TEST_ASSERT_EQUAL_INT(0, tokenizeLine(line, tokens));
        TEST_ASSERT_NULL(tokens[0]);
}

void test_tokenizeLine_whitespaceOnlyReturnsZero(void)
{
        char line[] = " \t  \n";
        char *tokens[LIMIT];
        TEST_ASSERT_EQUAL_INT(0, tokenizeLine(line, tokens));
}

void test_tokenizeLine_operatorsNeedSurroundingSpaces(void)
{
        char line[] = "echo a;echo b ; ls>out";
        char *tokens[LIMIT];
        TEST_ASSERT_EQUAL_INT(5, tokenizeLine(line, tokens));
        TEST_ASSERT_EQUAL_STRING("a;echo", tokens[1]);
        TEST_ASSERT_EQUAL_STRING(";", tokens[3]);
        TEST_ASSERT_EQUAL_STRING("ls>out", tokens[4]);
}

/* ---------- parseRedirection ---------- */

void test_parseRedirection_none(void)
{
        char *cmd[] = {"ls", "-l", NULL};
        char *args[8];
        int counter = -1;
        TEST_ASSERT_EQUAL_INT(0, parseRedirection(cmd, args, &counter));
        TEST_ASSERT_EQUAL_INT(2, counter);
        TEST_ASSERT_EQUAL_STRING("ls", args[0]);
        TEST_ASSERT_EQUAL_STRING("-l", args[1]);
        TEST_ASSERT_NULL(args[2]);
}

void test_parseRedirection_input(void)
{
        char *cmd[] = {"sort", "<", "in.txt", NULL};
        char *args[8];
        int counter = -1;
        TEST_ASSERT_EQUAL_INT(1, parseRedirection(cmd, args, &counter));
        TEST_ASSERT_EQUAL_INT(1, counter);
        TEST_ASSERT_EQUAL_STRING("sort", args[0]);
        TEST_ASSERT_NULL(args[1]);
        TEST_ASSERT_EQUAL_STRING("in.txt", cmd[counter + 1]);
}

void test_parseRedirection_output(void)
{
        char *cmd[] = {"echo", "hi", ">", "out.txt", NULL};
        char *args[8];
        int counter = -1;
        TEST_ASSERT_EQUAL_INT(2, parseRedirection(cmd, args, &counter));
        TEST_ASSERT_EQUAL_INT(2, counter);
        TEST_ASSERT_NULL(args[2]);
        TEST_ASSERT_EQUAL_STRING("out.txt", cmd[counter + 1]);
}

void test_parseRedirection_firstOperatorWins(void)
{
        char *cmd[] = {"cat", ">", "a", "<", "b", NULL};
        char *args[8];
        int counter = -1;
        TEST_ASSERT_EQUAL_INT(2, parseRedirection(cmd, args, &counter));
        TEST_ASSERT_EQUAL_INT(1, counter);
}

void test_parseRedirection_missingFileNameGivesNull(void)
{
        char *cmd[] = {"ls", ">", NULL};
        char *args[8];
        int counter = -1;
        TEST_ASSERT_EQUAL_INT(2, parseRedirection(cmd, args, &counter));
        TEST_ASSERT_NULL(cmd[counter + 1]);
}

/* ---------- parseBackground ---------- */

void test_parseBackground_none(void)
{
        char *args[] = {"sleep", "1", NULL};
        char *cmd[8];
        TEST_ASSERT_EQUAL_INT(0, parseBackground(args, cmd));
        TEST_ASSERT_EQUAL_STRING("sleep", cmd[0]);
        TEST_ASSERT_EQUAL_STRING("1", cmd[1]);
        TEST_ASSERT_NULL(cmd[2]);
}

void test_parseBackground_trailingAmpersand(void)
{
        char *args[] = {"sleep", "1", "&", NULL};
        char *cmd[8];
        TEST_ASSERT_EQUAL_INT(1, parseBackground(args, cmd));
        TEST_ASSERT_EQUAL_STRING("sleep", cmd[0]);
        TEST_ASSERT_NULL(cmd[2]);
}

void test_parseBackground_tokensAfterAmpersandDropped(void)
{
        char *args[] = {"a", "&", "b", NULL};
        char *cmd[8];
        TEST_ASSERT_EQUAL_INT(1, parseBackground(args, cmd));
        TEST_ASSERT_EQUAL_STRING("a", cmd[0]);
        TEST_ASSERT_NULL(cmd[1]);
}

void test_parseBackground_ampersandInsideTokenIgnored(void)
{
        char *args[] = {"sleep", "1&", NULL};
        char *cmd[8];
        TEST_ASSERT_EQUAL_INT(0, parseBackground(args, cmd));
        TEST_ASSERT_EQUAL_STRING("1&", cmd[1]);
}

/* ---------- singleProccessing (on plan) ---------- */

void test_singleProccessing_foregroundRunsAndReapsChild(void)
{
        char *args[] = {"sh", "-c", "echo fg", NULL};
        captureStart();
        int ret = singleProccessing(args, 0);
        const char *out = captureStop();
        TEST_ASSERT_EQUAL_INT(1, ret);
        TEST_ASSERT_EQUAL_STRING("fg\n", out);
        TEST_ASSERT_TRUE(noChildrenLeft());
}

void test_singleProccessing_unknownCommandStillReturnsOne(void)
{
        char *args[] = {"osproje_no_such_command_xyz", NULL};
        captureStart();
        int ret = singleProccessing(args, 0);
        captureStop();
        TEST_ASSERT_EQUAL_INT(1, ret);
        TEST_ASSERT_TRUE(noChildrenLeft());
}

/* ---------- singleProccessingBg / bgHandlerControl ---------- */

void test_singleProccessing_backgroundPrintsPidAndRetval(void)
{
        char *args[] = {"sh", "-c", "exit 7", NULL};
        captureStart();
        int ret = singleProccessing(args, 1);
        // handler cocugu toplayana kadar bekle (en fazla ~2 sn)
        for (int i = 0; i < 200 && !noChildrenLeft(); i++)
                usleep(10000);
        const char *out = captureStop();

        TEST_ASSERT_EQUAL_INT(1, ret);
        TEST_ASSERT_NOT_NULL(strstr(out, "Proses PID:"));
        TEST_ASSERT_NOT_NULL(strstr(out, "Degeriyle Olusturuldu\n"));
        TEST_ASSERT_NOT_NULL(strstr(out, "retval : 7"));
}

void test_singleProccessingBg_installsSigchldHandler(void)
{
        char *args[] = {"true", NULL};
        captureStart();
        singleProccessingBg(args);
        struct sigaction current;
        sigaction(SIGCHLD, NULL, &current);
        for (int i = 0; i < 200 && !noChildrenLeft(); i++)
                usleep(10000);
        captureStop();
        TEST_ASSERT_TRUE(current.sa_handler == bgHandlerControl);
        TEST_ASSERT_TRUE(current.sa_flags & SA_NOCLDSTOP);
}

void test_bgHandlerControl_reportsExitStatus(void)
{
        pid_t pid = fork();
        if (pid == 0)
                _exit(3);
        siginfo_t info;
        waitid(P_PID, pid, &info, WEXITED | WNOWAIT); // zombie olana kadar bekle, toplama

        captureStart();
        bgHandlerControl(SIGCHLD);
        const char *out = captureStop();

        char expected[64];
        snprintf(expected, sizeof(expected), "[%d] retval : 3 \n", pid);
        TEST_ASSERT_EQUAL_STRING(expected, out);
        TEST_ASSERT_TRUE(noChildrenLeft());
}

void test_bgHandlerControl_silentWhenNoChild(void)
{
        captureStart();
        bgHandlerControl(SIGCHLD);
        const char *out = captureStop();
        TEST_ASSERT_EQUAL_STRING("", out);
}

void test_bgHandlerControl_silentWhenChildKilledBySignal(void)
{
        pid_t pid = fork();
        if (pid == 0)
        {
                pause();
                _exit(0);
        }
        kill(pid, SIGKILL);
        siginfo_t info;
        waitid(P_PID, pid, &info, WEXITED | WNOWAIT);

        captureStart();
        bgHandlerControl(SIGCHLD);
        const char *out = captureStop();
        TEST_ASSERT_EQUAL_STRING("", out);
        TEST_ASSERT_TRUE(noChildrenLeft());
}

/* ---------- outputProcessing ---------- */

void test_outputProcessing_writesStdoutToFile(void)
{
        char *args[] = {"echo", "hello", "world", NULL};
        outputProcessing(args, tmpPath);
        TEST_ASSERT_EQUAL_STRING("hello world\n", readFile(tmpPath));
}

void test_outputProcessing_truncatesExistingFile(void)
{
        writeFile(tmpPath, "this is some much longer old content\n");
        char *args[] = {"echo", "new", NULL};
        outputProcessing(args, tmpPath);
        TEST_ASSERT_EQUAL_STRING("new\n", readFile(tmpPath));
}

void test_outputProcessing_createsFileWithMode0600(void)
{
        char *args[] = {"true", NULL};
        mode_t old = umask(0);
        outputProcessing(args, tmpPath);
        umask(old);
        struct stat st;
        TEST_ASSERT_EQUAL_INT(0, stat(tmpPath, &st));
        TEST_ASSERT_EQUAL_INT(0600, st.st_mode & 0777);
}

/* ---------- inputProcessing ---------- */

void test_inputProcessing_feedsFileToStdin(void)
{
        writeFile(tmpPath, "line1\nline2\n");
        char *args[] = {"cat", NULL};
        captureStart();
        inputProcessing(args, tmpPath);
        const char *out = captureStop();
        TEST_ASSERT_EQUAL_STRING("line1\nline2\n", out);
}

void test_inputProcessing_missingFilePrintsNotFound(void)
{
        char *args[] = {"cat", NULL};
        captureStart();
        inputProcessing(args, "/tmp/osproje_definitely_missing_file");
        const char *out = captureStop();
        TEST_ASSERT_EQUAL_STRING("/tmp/osproje_definitely_missing_file Not Found!!\n", out);
        TEST_ASSERT_TRUE(noChildrenLeft());
}

/* ---------- execPipe ---------- */

void test_execPipe_twoCommands(void)
{
        char *args[] = {"echo", "hello", "|", "tr", "a-z", "A-Z", NULL};
        captureStart();
        int ret = execPipe(args);
        const char *out = captureStop();
        TEST_ASSERT_EQUAL_INT(0, ret);
        TEST_ASSERT_EQUAL_STRING("HELLO\n", out);
        TEST_ASSERT_TRUE(noChildrenLeft());
}

void test_execPipe_multiLineData(void)
{
        char *args[] = {"printf", "b\\na\\nc\\n", "|", "sort", NULL};
        captureStart();
        int ret = execPipe(args);
        const char *out = captureStop();
        TEST_ASSERT_EQUAL_INT(0, ret);
        TEST_ASSERT_EQUAL_STRING("a\nb\nc\n", out);
}

int main(void)
{
        UNITY_BEGIN();

        RUN_TEST(test_isPipe_returnsZeroWithoutPipe);
        RUN_TEST(test_isPipe_returnsOneWithPipe);
        RUN_TEST(test_isPipe_emptyCommand);
        RUN_TEST(test_isPipe_ignoresPipeInsideToken);

        RUN_TEST(test_tokenizeLine_splitsOnSpacesTabsNewline);
        RUN_TEST(test_tokenizeLine_emptyLineReturnsZero);
        RUN_TEST(test_tokenizeLine_whitespaceOnlyReturnsZero);
        RUN_TEST(test_tokenizeLine_operatorsNeedSurroundingSpaces);

        RUN_TEST(test_parseRedirection_none);
        RUN_TEST(test_parseRedirection_input);
        RUN_TEST(test_parseRedirection_output);
        RUN_TEST(test_parseRedirection_firstOperatorWins);
        RUN_TEST(test_parseRedirection_missingFileNameGivesNull);

        RUN_TEST(test_parseBackground_none);
        RUN_TEST(test_parseBackground_trailingAmpersand);
        RUN_TEST(test_parseBackground_tokensAfterAmpersandDropped);
        RUN_TEST(test_parseBackground_ampersandInsideTokenIgnored);

        RUN_TEST(test_singleProccessing_foregroundRunsAndReapsChild);
        RUN_TEST(test_singleProccessing_unknownCommandStillReturnsOne);
        RUN_TEST(test_singleProccessing_backgroundPrintsPidAndRetval);
        RUN_TEST(test_singleProccessingBg_installsSigchldHandler);

        RUN_TEST(test_bgHandlerControl_reportsExitStatus);
        RUN_TEST(test_bgHandlerControl_silentWhenNoChild);
        RUN_TEST(test_bgHandlerControl_silentWhenChildKilledBySignal);

        RUN_TEST(test_outputProcessing_writesStdoutToFile);
        RUN_TEST(test_outputProcessing_truncatesExistingFile);
        RUN_TEST(test_outputProcessing_createsFileWithMode0600);

        RUN_TEST(test_inputProcessing_feedsFileToStdin);
        RUN_TEST(test_inputProcessing_missingFilePrintsNotFound);

        RUN_TEST(test_execPipe_twoCommands);
        RUN_TEST(test_execPipe_multiLineData);

        return UNITY_END();
}
