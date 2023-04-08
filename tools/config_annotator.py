#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: DVM Host Configuration Annotator
Creator: K4YT3X
Date Created: April 1, 2023
Last Modified: April 1, 2023

This script updates DVM Host's example config with the content of a modified one
to generate a configuration with the settings of the modified config but with the
config from the original example config.

Example usages:

1. Annotate a config file in-place using DVM Host's default config file:

`python config_annotator.py -em modified.yaml`

2. Annotate a config file using a custom template file and export it to a separate file:

`python config_annotator.py -t template.yml -m modified.yml -o annotated.yml`
"""
import argparse
import sys
from pathlib import Path
from typing import Dict

# since this stand-alone script does not have a pyproject.yaml or requirements.txt
# prompt the user to install the required libraries if they're not found
try:
    import requests
    import ruamel.yaml
except ImportError:
    print(
        "Error: unable to import ruamel.yaml or requests\n"
        "Please install it with `pip install ruamel.yaml requests`",
        file=sys.stderr,
    )
    sys.exit(1)

# URL to DVM Host's official example URL
EXAMPLE_CONFIG_URL = (
    "https://github.com/DVMProject/dvmhost/raw/master/configs/config.example.yml"
)


def parse_arguments() -> argparse.Namespace:
    """
    parse command line arguments

    :return: parsed arguments in an argparse namespace object
    """
    parser = argparse.ArgumentParser(description="Update a YAML file with new values.")
    template_group = parser.add_mutually_exclusive_group(required=True)
    template_group.add_argument(
        "-e",
        "--example",
        help="use DVM Host's default example config as template",
        action="store_true",
    )
    template_group.add_argument(
        "-t",
        "--template",
        type=Path,
        help="path to the template config YAML file with comments",
    )
    parser.add_argument(
        "-m", "--modified", type=Path, help="path to the modified YAML file"
    )
    parser.add_argument(
        "-o", "--output", type=Path, help="path to the save the annotated YAML file"
    )
    return parser.parse_args()


def update_dict(template_dict: Dict, modified_dict: Dict) -> Dict:
    """
    Recursively updates the values of template_dict with the values of modified_dict,
    without changing the order of the keys in template_dict.

    :param template_dict: the dictionary to update.
    :type template_dict: dict
    :param modified_dict: the dictionary with the updated values
    :type modified_dict: dict
    :return: the updated dictionary
    :rtype: dict
    """
    for key, value in modified_dict.items():
        if isinstance(value, dict) and key in template_dict:
            update_dict(template_dict[key], value)
        elif key in template_dict:
            template_dict[key] = value
    return template_dict


def main(
    example: bool, template_path: Path, modified_path: Path, output_path: Path
) -> None:
    # if the example file is to be used
    # download it from the GitHub repo
    if example is True:
        response = requests.get(EXAMPLE_CONFIG_URL)
        response.raise_for_status()
        template_content = response.text

    # if a custom template file is to be used
    else:
        with template_path.open(mode="r") as template_file:
            template_content = template_file.read()

    # load the YAML file
    template = ruamel.yaml.YAML().load(template_content)

    # load the modified YAML file
    with modified_path.open(mode="r") as modified_file:
        modified = ruamel.yaml.YAML().load(modified_file)

    # update the template with the modified YAML data
    update_dict(template, modified)

    # if no output file path is specified, write it back to the modified file
    if args.output is None:
        write_path = modified_path

    # if an output file path has been specified, write the output to that file
    else:
        write_path = output_path

    # write the updated YAML file back to disk
    with write_path.open(mode="w") as output_file:
        yaml = ruamel.yaml.YAML()

        # set the output YAML file's indentation to 4 spaces per project standard
        yaml.indent(mapping=4, sequence=6, offset=4)
        yaml.dump(template, output_file)


if __name__ == "__main__":
    args = parse_arguments()
    main(args.example, args.template, args.modified, args.output)
