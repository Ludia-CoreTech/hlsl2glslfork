/**
 * HLSL -> GLSL ES command line converter
 */
#include "hlsl2glsl.h"
#include <string>
#include <stdexcept>
#include <iostream>

void ReplaceString(std::string& target, const std::string& search, const std::string& replace, size_t startPos)
{
	if (search.empty())
		return;

	std::string::size_type p = startPos;
	while ((p = target.find (search, p)) != std::string::npos)
	{
		target.replace (p, search.size (), replace);
		p += replace.size ();
	}
}

bool ReadStringFromFile(const std::string& fname, std::string& output)
{
	FILE* file = fopen(fname.c_str(), "rb" );
	if (file == NULL)
		return false;

	fseek(file, 0, SEEK_END);
	const long length = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (length < 0)
	{
		fclose( file );
		return false;
	}

	output.resize(length);
	const size_t readLength = fread(&*output.begin(), 1, length, file);

	fclose(file);

	if (readLength != length)
	{
		output.clear();
		return false;
	}

	ReplaceString(output, "\r\n", "\n", 0);

	return true;
}

void WriteStringToFile(const char* pathName, const std::string& text)
{
	FILE* f = fopen(pathName, "wb");
	fwrite (text.c_str(), 1, text.size(), f);
	fclose (f);
}

void ConvertAndWriteFile(const std::string& iname, const std::string &oname,
					const char* entryPoint, EShLanguage lang)
{
	std::string hlsl;
	if (!ReadStringFromFile(iname.c_str(), hlsl))
		throw std::runtime_error("Could not load file " + iname);

	const ETargetVersion version = ETargetGLSL_ES_100;
	ShHandle compiler = Hlsl2Glsl_ConstructCompiler(lang);
	const unsigned int options = ETranslateOpAvoidBuiltinAttribNames;
	Hlsl2Glsl_UseUserVaryings(compiler, true);
	const char* sourceStr = hlsl.c_str();

	const int parseOk = Hlsl2Glsl_Parse(compiler, hlsl.c_str(), version, options);
	const char* infoLog = Hlsl2Glsl_GetInfoLog(compiler);
	if (parseOk)
	{
		const int translateOk = Hlsl2Glsl_Translate(compiler, entryPoint, version, options);
		infoLog = Hlsl2Glsl_GetInfoLog(compiler);
		if (translateOk)
		{
			const std::string source = Hlsl2Glsl_GetShader(compiler);
			//std::cout << source << std::endl;
			WriteStringToFile(oname.c_str(), source);
		}
		else
		{
			std::cerr << infoLog << std::endl;
			throw std::runtime_error("Could not translate " + iname);
		}
	}
	else
	{
		std::cerr << infoLog << std::endl;
		throw std::runtime_error("Could not parse");
	}

	Hlsl2Glsl_DestructCompiler(compiler);
}

void PrintUsage()
{
	std::cout << "-f|v <entrypoint> <inputfile> <outputfile>" << std::endl;
	std::cout << "\t-v: vertex shader" << std::endl;
	std::cout << "\t-f: fragment shader" << std::endl;
	std::cout << "\tentry point: usually VS or FS" << std::endl;
}

int main(int argc, char** argv)
{
	if (argc != 5)
	{
		std::cout << "Invalid number of arguments" << std::endl;
		PrintUsage();
		return 1;
	}

	bool vertex = false;
	if (argv[1][0] == '-')
		if (0 == strcmp("-v", argv[1]))
			vertex = true;
		if (0 == strcmp("-f", argv[1]))
			vertex = false;

	const std::string entryPoint   = argv[2];
	const std::string inputFile    = argv[3];
	const std::string outputFile   = argv[4];

	if (0 == inputFile.compare(outputFile))
	{
		std::cerr << "Input and output cannot be the same" << std::endl;
		return 1;
	}

	const EShLanguage lang = vertex ? EShLangVertex : EShLangFragment;

	Hlsl2Glsl_Initialize();

	try
	{
		ConvertAndWriteFile(inputFile, outputFile, entryPoint.c_str(), lang);
		std::cout << "Converted " << inputFile.c_str() << " to " << outputFile.c_str() << std::endl;
	}
	catch (std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
		Hlsl2Glsl_Shutdown();
		return 1;
	}

	Hlsl2Glsl_Shutdown();
	return 0;
}