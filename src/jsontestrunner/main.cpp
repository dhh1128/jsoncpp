// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

/* This executable is used for testing parser/writer using real JSON files.
 */

#include <json/json.h>
#include <algorithm> // sort
#include <sstream>
#include <stdio.h>

#if defined(_MSC_VER) && _MSC_VER >= 1310
#pragma warning(disable : 4996) // disable fopen deprecation warning
#endif

struct options
{
  std::string path;
  json::features features;
  bool parse_only;
  typedef std::string (*write_func)(json::value const&);
  write_func write;
};

static std::string normalize_floating_point_str(double value) {
  char buffer[32];
#if defined(_MSC_VER) && defined(__STDC_SECURE_LIB__)
  sprintf_s(buffer, sizeof(buffer), "%.16g", value);
#else
  snprintf(buffer, sizeof(buffer), "%.16g", value);
#endif
  buffer[sizeof(buffer) - 1] = 0;
  std::string s(buffer);
  std::string::size_type index = s.find_last_of("eE");
  if (index != std::string::npos) {
	std::string::size_type hasSign =
		(s[index + 1] == '+' || s[index + 1] == '-') ? 1 : 0;
	std::string::size_type exponent_start_index = index + 1 + hasSign;
	std::string normalized = s.substr(0, exponent_start_index);
	std::string::size_type index_digit =
		s.find_first_not_of('0', exponent_start_index);
	std::string exponent = "0";
	if (index_digit !=
		std::string::npos) // There is an exponent different from 0
	{
	  exponent = s.substr(index_digit);
	}
	return normalized + exponent;
  }
  return s;
}

static std::string read_input_test_file(const char* path) {
  FILE* file = fopen(path, "rb");
  if (!file)
	return std::string("");
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  std::string text;
  char* buffer = new char[size + 1];
  buffer[size] = 0;
  if (fread(buffer, 1, size, file) == (unsigned long)size)
	text = buffer;
  fclose(file);
  delete[] buffer;
  return text;
}

static void
print_value_tree(FILE* fout, json::value& value, std::string const & path = ".") {
  if (value.has_comment(json::comment_before)) {
	fprintf(fout, "%s\n", value.get_comment(json::comment_before).c_str());
  }
  switch (value.type()) {
  case json::vt_null:
	fprintf(fout, "%s=null\n", path.c_str());
	break;
  case json::vt_int:
	fprintf(fout,
			"%s=%s\n",
			path.c_str(),
			json::valueToString(value.as_largest_int()).c_str());
	break;
  case json::vt_uint:
	fprintf(fout,
			"%s=%s\n",
			path.c_str(),
			json::valueToString(value.as_largest_uint()).c_str());
	break;
  case json::vt_real:
	fprintf(fout,
			"%s=%s\n",
			path.c_str(),
			normalize_floating_point_str(value.as_double()).c_str());
	break;
  case json::vt_string:
	fprintf(fout, "%s=\"%s\"\n", path.c_str(), value.as_string().c_str());
	break;
  case json::vt_bool:
	fprintf(fout, "%s=%s\n", path.c_str(), value.as_bool() ? "true" : "false");
	break;
  case json::vt_array: {
	fprintf(fout, "%s=[]\n", path.c_str());
	int size = value.size();
	for (int index = 0; index < size; ++index) {
	  static char buffer[16];
#if defined(_MSC_VER) && defined(__STDC_SECURE_LIB__)
	  sprintf_s(buffer, sizeof(buffer), "[%d]", index);
#else
	  snprintf(buffer, sizeof(buffer), "[%d]", index);
#endif
	  print_value_tree(fout, value[index], path + buffer);
	}
  } break;
  case json::vt_object: {
	fprintf(fout, "%s={}\n", path.c_str());
	json::value::Members members(value.get_member_names());
	std::sort(members.begin(), members.end());
	std::string suffix = *(path.end() - 1) == '.' ? "" : ".";
	for (json::value::Members::iterator it = members.begin();
		 it != members.end();
		 ++it) {
	  std::string const & name = *it;
	  print_value_tree(fout, value[name], path + suffix + name);
	}
  } break;
  default:
	break;
  }

  if (value.has_comment(json::comment_after)) {
	fprintf(fout, "%s\n", value.get_comment(json::comment_after).c_str());
  }
}

static int parseAndSaveValueTree(std::string const & input,
								 std::string const & actual,
								 std::string const & kind,
								 const json::features& features,
								 bool parse_only,
								 json::value* root)
{
  json::reader reader(features);
  bool parsing_successful = reader.parse(input, *root);
  if (!parsing_successful) {
	printf("Failed to parse %s file: \n%s\n",
		   kind.c_str(),
		   reader.get_formatted_messages().c_str());
	return 1;
  }
  if (!parse_only) {
	FILE* factual = fopen(actual.c_str(), "wt");
	if (!factual) {
	  printf("Failed to create %s actual file.\n", kind.c_str());
	  return 2;
	}
	print_value_tree(factual, *root);
	fclose(factual);
  }
  return 0;
}
// static std::string useFastWriter(json::value const& root) {
//   json::fast_writer writer;
//   writer.enableYAMLCompatibility();
//   return writer.write(root);
// }
static std::string use_styled_writer(
	json::value const& root)
{
  json::styled_writer writer;
  return writer.write(root);
}
static std::string use_styled_stream_writer(
	json::value const& root)
{
  json::styled_stream_writer writer;
  std::ostringstream sout;
  writer.write(sout, root);
  return sout.str();
}
static std::string use_built_styled_stream_writer(
	json::value const& root)
{
  json::stream_writer_builder builder;
  return json::write_string(builder, root);
}
static int rewrite_value_tree(
	std::string const & rewrite_path,
	const json::value& root,
	options::write_func write,
	std::string* rewrite)
{
  *rewrite = write(root);
  FILE* fout = fopen(rewrite_path.c_str(), "wt");
  if (!fout) {
	printf("Failed to create rewrite file: %s\n", rewrite_path.c_str());
	return 2;
  }
  fprintf(fout, "%s\n", rewrite->c_str());
  fclose(fout);
  return 0;
}

static std::string remove_suffix(std::string const & path,
								std::string const & extension) {
  if (extension.length() >= path.length())
	return std::string("");
  std::string suffix = path.substr(path.length() - extension.length());
  if (suffix != extension)
	return std::string("");
  return path.substr(0, path.length() - extension.length());
}

static void print_config() {
// Print the configuration used to compile JsonCpp
#if defined(JSON_NO_INT64)
  printf("JSON_NO_INT64=1\n");
#else
  printf("JSON_NO_INT64=0\n");
#endif
}

static int print_usage(const char* argv[]) {
  printf("Usage: %s [--strict] input-json-file\n", argv[0]);
  return 3;
}

static int parse_cmdline(
	int argc, const char* argv[], options* opts)
{
  opts->parse_only = false;
  opts->write = &use_styled_writer;
  if (argc < 2) {
	return print_usage(argv);
  }
  int index = 1;
  if (std::string(argv[index]) == "--json-checker") {
	opts->features = json::features::strict_mode();
	opts->parse_only = true;
	++index;
  }
  if (std::string(argv[index]) == "--json-config") {
	print_config();
	return 3;
  }
  if (std::string(argv[index]) == "--json-writer") {
	++index;
	std::string const writer_name(argv[index++]);
	if (writer_name == "styled_writer") {
	  opts->write = &use_styled_writer;
	} else if (writer_name == "styled_stream_writer") {
	  opts->write = &use_styled_stream_writer;
	} else if (writer_name == "built_styled_stream_writer") {
	  opts->write = &use_built_styled_stream_writer;
	} else {
	  printf("Unknown '--json-writer %s'\n", writer_name.c_str());
	  return 4;
	}
  }
  if (index == argc || index + 1 < argc) {
	return print_usage(argv);
  }
  opts->path = argv[index];
  return 0;
}


static int run_test(options const& opts)
{
  int exitCode = 0;

  std::string input = read_input_test_file(opts.path.c_str());
  if (input.empty()) {
	printf("Failed to read input or empty input: %s\n", opts.path.c_str());
	return 3;
  }

  std::string basePath = remove_suffix(opts.path, ".json");
  if (!opts.parse_only && basePath.empty()) {
	printf("Bad input path. path does not end with '.expected':\n%s\n",
			opts.path.c_str());
	return 3;
  }

  std::string const actual_path = basePath + ".actual";
  std::string const rewrite_path = basePath + ".rewrite";
  std::string const rewrite_actual_path = basePath + ".actual-rewrite";

  json::value root;
  exitCode = parseAndSaveValueTree(
	  input, actual_path, "input",
	  opts.features, opts.parse_only, &root);
  if (exitCode || opts.parse_only) {
	return exitCode;
  }
  std::string rewrite;
  exitCode = rewrite_value_tree(rewrite_path, root, opts.write, &rewrite);
  if (exitCode) {
	return exitCode;
  }
  json::value rewriteRoot;
  exitCode = parseAndSaveValueTree(
	  rewrite, rewrite_actual_path, "rewrite",
	  opts.features, opts.parse_only, &rewriteRoot);
  if (exitCode) {
	return exitCode;
  }
  return 0;
}
int main(int argc, const char* argv[]) {
  options opts;
  int exitCode = parse_cmdline(argc, argv, &opts);
  if (exitCode != 0) {
	printf("Failed to parse command-line.\n");
	return exitCode;
  }
  try {
	return run_test(opts);
  }
  catch (const std::exception& e) {
	printf("Unhandled exception:\n%s\n", e.what());
	return 1;
  }
}
