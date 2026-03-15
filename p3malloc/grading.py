import subprocess
import os

FOLDER_PATH = os.getcwd()

trace_weights = {
    "fail1.txt": 2,
    "fail2.txt": 2,
    "short1-bal.txt": 2,
    "short2-bal.txt": 2,
    "free-1.txt": 2,
    "free-2.txt": 2,
    "free-3.txt": 2,
    "free-4.txt": 2,
    "coalesce.txt": 4,
    "amptjp-bal.txt": 8,
    "binary-bal.txt": 8,
    "cccp-bal.txt": 8,
    "coalescing-bal.txt": 8,
    "cp-decl-bal.txt": 8,
    "random-bal.txt": 10,
}


try:
   os.makedirs("outputs/")
except FileExistsError:
   # directory already exists
   pass


results = []
total_grade = 0
total_weights = 0

results.append("|{:<20}|{:^50} |{:^17}".format(
        "", 
        "Correctness",
        "Score",
        ))
results.append("|{:<20}|{:^13}|{:^13}|{:^13}| {:^7} | {:^6} | {:^6}".format(
        "Trace", 
        "Success",
        "Offset",
        "Hops",
        "Avg",
        "Weight",
        "Score",
        ))
results.append("|--------------------|---------------------------------------------------|----------------")

for trace in trace_weights:
    output_path = "outputs/" + trace.replace("txt", "out")
    expected_output_path = "expected_outputs/" + trace.replace("txt", "out")
    try:
        subprocess.run(["./test", "-t", "traces/"+trace, "-o", output_path], timeout=15)
    except subprocess.TimeoutExpired:
        print('test {} takes too long... Probably an infinite loop happens'.format(trace))
    lines = []
    expected_lines = []
    with open(output_path, 'r') as f:
        lines = f.readlines()
    with open(expected_output_path, 'r') as f:
        expected_lines = f.readlines()
    num_success_correct = 0
    num_offset_correct = 0
    num_hops_correct = 0

    for i in range(len(expected_lines)):
        if i >= len(lines):
            break

        line = lines[i]
        status = line.strip().split('\t')
        success = offset = hops = -2
        if len(status) >= 1:
            try:
                success = int(status[0])
            except ValueError:
                success = -2
        if len(status) >= 2:
            try:
                offset = int(status[1])
            except ValueError:
                offset = -2
        if len(status) >= 3:
            try:
                hops = int(status[2])
            except ValueError:
                offset = -2

        expected_line = expected_lines[i]
        expected_status = expected_line.strip().split('\t')
        expected_success = int(expected_status[0])
        expected_offset = int(expected_status[1])
        expected_hops = int(expected_status[2])

        if success == expected_success:
            num_success_correct += 1
        if offset == expected_offset:
            num_offset_correct += 1
        if hops == expected_hops:
            num_hops_correct += 1
    
    correctness = (num_success_correct + num_offset_correct + num_hops_correct) / (len(expected_lines) * 3)
    weight = trace_weights[trace]
    grade = correctness * weight
    total_grade += grade
    total_weights += weight
    results.append("|{:<20}|{:>6}/{:<6}|{:>6}/{:<6}|{:>6}/{:<6}| {:>6}% | {:>6} | {:>6}".format(
        trace, 
        num_success_correct, len(expected_lines),
        num_offset_correct, len(expected_lines),
        num_hops_correct, len(expected_lines),
        "{:.2f}".format(correctness*100),
        weight,
        "{:.2f}".format(grade),
        ))

results.append("|--------------------|---------------------------------------------------|----------------")

results.append("|{:<20}|{:^50} | {:>6}   {:>6}".format(
        "Total", 
        "",
        "",
        "{:.2f}".format(total_grade),
        ))
        
for result in results:
    print(result)
print()
print("Your grade: {}/{}".format("{:.2f}".format(total_grade), total_weights))

with open("00-grade-malloc.txt", 'w') as f:
    f.write("malloc project grade: {}/{}\n".format("{:.2f}".format(total_grade), total_weights))
    f.write("\nDetails:\n")
    for result in results:
        f.write(result)
        f.write("\n")

# Checks and reminds student to do incremental commits
def checkIncrementalCommit():
    incr_file = os.path.join(FOLDER_PATH, 'contrib-commits.txt')
    if not os.path.isfile(incr_file):
        print("WARNING: Incremental commit file 'contrib-commits.txt' not found in project root directory.")
        return 
    # If exist, check for at least 10 lines.
    with open(incr_file, 'r') as f:
        lines = f.readlines()
        if len(lines) < 10:
            print("WARNING: Your incremental commit file 'contrib-commits.txt' has less than 10 lines")
            print("Please remember to make incremental commits to checkpoint your progress and prevent score deduction.")
            print("Every time you pass a test, do `git commit` and `git push`, copy the link from GitLab, then paste it as an extra line.")
            print("Format of a line should be (expected score, github commit link, description) pair:")
            print("e.g. 14, https://proj.cs.uchicago.edu/cmsc-14400-winter-2026/raniayu/-/commit/COMMIT_HASH, pass fail1\n")

def checkAcademicHonesty():
    ah_file = os.path.join(FOLDER_PATH, 'academic-honesty.txt')
    if not os.path.isfile(ah_file):
        print("WARNING: Academic honesty file 'academic-honesty.txt' not found in project root directory.")
        print("Please include this file with your submission to avoid score deduction.")
        return

    if os.path.isfile(ah_file):
        with open(ah_file, 'r') as f:
            content = f.read()
            if 'CNET' in content or 'cnet' in content:
                print("WARNING: Please fill in your CNET ID in the 'academic-honesty.txt' file.\n")
                return

checkIncrementalCommit()
print()
checkAcademicHonesty()