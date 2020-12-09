#include <fstream>
#include <iostream>
#include <regex>
#include <vector>
#include <cstdlib>

namespace
{

class mutation_generator
{
public:
  std::string const red_color   = "\x1B[31m";
  std::string const reset_color = "\033[0m";

public:
  explicit mutation_generator( std::string const& filename )
    : source_filename( filename )
  {
    /* define some mutation operators */
    patterns.emplace_back( R"(1'b0)", std::vector<std::string>{ "1'b1" } );
    patterns.emplace_back( R"(1'b1)", std::vector<std::string>{ "1'b0" } );

    patterns.emplace_back( R"(~)", std::vector<std::string>{ "" } );
    patterns.emplace_back( R"(==)", std::vector<std::string>{ "!=" } );
    patterns.emplace_back( R"(>)", std::vector<std::string>{ ">=", "<", "<=", "==", "!=" } );
    patterns.emplace_back( R"(&)", std::vector<std::string>{ "|", "^" } );
    patterns.emplace_back( R"(\|)", std::vector<std::string>{ "&", "^" } );
    patterns.emplace_back( R"(\^)", std::vector<std::string>{ "|", "&" } );
  }

public:
  void run()
  {
    load_file();
    generate_mutants();
    // compile_mutants();
  }

private:
  void load_file()
  {
    /* read the whole file */
    std::ifstream in( source_filename.c_str() );
    std::string line;
    while ( std::getline( in, line ) )
    {
      lines.emplace_back( line );
    }
    in.close();
  }

  void generate_mutants()
  {
    uint32_t line_number{0};
    for ( auto const& line : lines )
    {
      for ( auto const& p : patterns )
      {
        std::smatch m;
        if ( std::regex_search( line, m, std::regex( p.first ) ) )
        {
          for ( const auto& r : p.second )
          {
            std::cout << source_filename << ":" << line_number << ":" << m.position() << ": ";
            std::cout << red_color << p.first << reset_color << ": " << line.substr( 0, m.position() )
                      << red_color << r << reset_color
                      << line.substr( m.position() + m.length() ) << std::endl;

            std::string const new_line =
              line.substr( 0, m.position() ) + r + line.substr( m.position() + m.length() );

            write_mutant( "mutant" + std::to_string( mutant_counter++ ) + ".v", line_number, new_line );
          }
        }
      }

      ++line_number;
    }
  }

  void write_mutant( std::string const& filename, uint32_t line_number, std::string const& new_line )
  {
    std::ofstream os( filename );
    uint32_t counter{0};
    for ( const auto& line : lines )
    {
      if ( counter++ == line_number )
      {
        os << new_line << '\n';
      }
      else
      {
        os << line << '\n';
      }
    }
    os.close();
  }

  void compile_mutants()
  {
    /* yosys -qp "synth -flatten; abc -g AND; write_aiger OUTPUT.aig" FILENAME */
    for ( auto i = 0u; i < mutant_counter; ++i )
    {
      std::string const command = "yosys -qp \"synth -flatten; abc -g AND; write_aiger output" + std::to_string( i ) + ".aig\" mutant" + std::to_string( i ) + ".v";
      std::cout << "Compile mutant " << i << std::endl;
      system( command.c_str() );
    }
  }

private:
  std::string source_filename;

  std::vector<std::string> lines;
  uint32_t mutant_counter{0};

  /* match-replace patterns */
  std::vector<std::pair<std::string, std::vector<std::string>>> patterns;
};

} // namespace mutation_generator

int main( int argc, char* argv[] )
{
  if ( argc != 2 )
  {
    std::cout << "usage: " << argv[0] << " <filename>" << std::endl;
    return -1;
  }

  mutation_generator gen( argv[1] );
  gen.run();
  return 0;
}
