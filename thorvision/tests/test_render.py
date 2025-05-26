import subprocess
import argparse
import os
import time


# def launch_instances(max_instances, resolution, log_file):
#     processes = []

#     for instance in range(1, max_instances + 1):
#         path = "./build/Release/test_main.app/Contents/MacOS/test_main"
#         cmd = [path, str(instance), resolution]
#         process = subprocess.Popen(cmd, stdout=log_file, stderr=subprocess.STDOUT)
#         processes.append(process)

#     return processes


def terminate_instances(processes):
    for p in processes:
        p.terminate()
    for p in processes:
        p.wait()
    processes.clear()


def run_scaling_test(resolutions, max_instances, duration=5):
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    run_dir = f"test_render_logs_{timestamp}"
    os.makedirs(run_dir, exist_ok=True)

    for res in resolutions:
        print(f"\n--- Testing resolution {res} ---")

        for instance in range(1, max_instances + 1):
            log_name = f"render_{res.replace('x', '_')}_{instance}.log"
            log_path = os.path.join(run_dir, log_name)
            processes = []

            print(
                f"\n--- Running {instance} instance(s) at {res} for {duration} seconds ---"
            )

            with open(log_path, "w") as log_file:

                path = "./build/Release/test_main.app/Contents/MacOS/test_main"
                cmd = [path, str(instance), res]
                process = subprocess.Popen(
                    cmd, stdout=log_file, stderr=subprocess.STDOUT
                )
                processes.append(process)

                # processes = launch_instances(instance, res, log_file)
                time.sleep(duration)
                terminate_instances(processes)


def main():
    parser = argparse.ArgumentParser(description="Scaling test for test_main app.")
    parser.add_argument(
        "--max-instances",
        type=int,
        default=4,
        help="Maximum number of concurrent instances to test.",
    )
    parser.add_argument(
        "--duration",
        type=int,
        default=5,
        help="Duration in seconds to run each instance group.",
    )
    parser.add_argument(
        "--res",
        nargs="+",
        required=True,
        help="List of resolutions to test, e.g. 640x480 1280x720",
    )

    args = parser.parse_args()
    run_scaling_test(args.res, args.max_instances, args.duration)


if __name__ == "__main__":
    main()
