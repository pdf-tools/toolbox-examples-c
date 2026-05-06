About this kit
==============

This kit contains the AddMetadata sample for PdfTools SDK for C. Pdftools SDK is a development library that lets you integrate PDF processing into your applications. For more information, review the Pdftools [documentation portal](https://www.pdf-tools.com/docs/).

By downloading and using this kit, you accept the Pdftools [license agreement](https://www.pdf-tools.com/license-agreement/) and [privacy policy](https://www.pdf-tools.com/privacy-policy/), and you allow Pdftools to track your usage data.

## Quick start

Follow these steps to build and run the sample with CMake.

### Prerequisites

- CMake version VERSION 3.16 or later

Use CMake to produce a makefile that compiles the sample `toolboxaddmetadata`.
The input configuration file for CMake is `CMakeLists.txt`.
CMake links native libraries and includes header files automatically. Supported platforms are Windows, Linux, and macOS.

### Run the sample

1. Navigate to the directory where `CMakeLists.txt` resides.
2. Run: `cmake .`
3. Run: `cmake --build .`
4. Run the sample: `./toolboxaddmetadata <inputPath> <outputPath> [<mdatafile>]`

## Licensing

- **Pdftools SDK** doesn't require a license key for evaluation. Without a license key, the SDK adds a watermark to output files.
- **Toolbox add-on** requires a trial or full license key to run. Without a valid license key, processing fails.

**Important:** Toolbox add-on processing fails without a valid license key.

To get a trial license key, create a user account at the [Pdftools portal](https://portal.pdf-tools.com/). For more information, refer to [Trial license overview](https://www.pdf-tools.com/docs/licenses/products/pdf-tools-sdk-license/#trial-license-overview).

## Technical support

Do you need technical support or want to report an issue?
Open a ticket through the [support form](https://www.pdf-tools.com/docs/support/).