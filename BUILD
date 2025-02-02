# ARCHIVE_NAME = self.name + "-" + self.version

ARCHIVE_NAME = "memory-wrapper-0.1.0"

filegroup(
    name = "all_files",
    srcs = glob(
        [
            "*",
        ],
        exclude = [
            ".git",
            "bazel-*",
        ] + subpackages(include = ["**"]),
        exclude_directories = 0,
    ),
)

genrule(
    name = "zip_archive",
    srcs = [":all_files"],
    outs = [ARCHIVE_NAME + ".zip"],
    cmd = "mkdir " + ARCHIVE_NAME + " && " +
          "cp -R -t " + ARCHIVE_NAME + "/ $(SRCS) && " +
          "zip -r $@ " + ARCHIVE_NAME,
)

genrule(
    name = "tar_gz_archive",
    srcs = [":all_files"],
    outs = ["memory-wrapper.tar.gz"],
    cmd = "tar -czvf $@ $(SRCS)",
)
