#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <vector>
#include <sys/wait.h>

namespace
{

struct token
{
  std::string lexem;
  uint32_t line;
  uint32_t column;
}; /* token */

std::vector<token> tokenize_line( std::string const& line, uint32_t line_number )
{
  std::vector<token> result;

  token t;;
  for ( uint32_t column_number = 0u; column_number < line.size(); ++column_number )
  {
    char const ch = line.at( column_number );
    if ( ch == ' ' ) /* skip whitespace */
    {
      if ( t.lexem != "" )
      {
        t.line = line_number;
        result.emplace_back( t );
        t.lexem.clear();
      }
      continue;
    }
    else if ( ( ch == ';' || ch == '~' || ch == ',' || ch == '(' || ch == ')' ) && ( t.lexem != "" ) ) /* split token */
    {
      if ( t.lexem != "" )
      {
        t.line = line_number;
        result.emplace_back( t );
        t.lexem.clear();
      }
      --column_number; /* let's read the same character in the next iteration */
      continue;
    }
    else
    {
      if ( t.lexem == "" )
      {
        t.column = column_number;
      }
      t.lexem += ch;
    }
  }

  if ( t.lexem != "" )
  {
    t.line = line_number;
    result.emplace_back( t );
  }

  return result;
}

bool is_keyword( std::string const s )
{
  assert( !s.empty() );
  return ( s == "module" ) || ( s.length() >= 4u && s.substr( 0, 4 ) == "wire" ) || ( s == "always" ) ||
    ( s == "if" ) || ( s == "else" ) || ( s == "begin" ) || ( s == "end" ) ||
    ( s == "input" ) || ( s == "output" ) || ( s.length() >= 3u && s.substr( 0, 3 ) == "reg" ) ||
    ( s == "posedge" ) || ( s == "endmodule" );
}

bool is_operator( std::string const s )
{
  assert( !s.empty() );
  return ( s == "{" ) || ( s == "}" ) || ( s == "@" ) || ( s == ";" ) ||
    ( s == "," ) || ( s == "(" ) || ( s == ")" ) || ( s == "&" ) ||
    ( s == "|" ) || ( s == "=" ) || ( s == "<=" ) || ( s == "<" ) ||
    ( s == ">" ) || ( s == ">=" ) || ( s == "||" ) || ( s == "&&" ) ||
    ( s == "^^" ) || ( s == "==" ) || ( s[0] == '[' );
}

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

    patterns.emplace_back( R"(!)", std::vector<std::string>{ "" } );
    patterns.emplace_back( R"(~)", std::vector<std::string>{ "" } );
    patterns.emplace_back( R"(==)", std::vector<std::string>{ "!=" } );
    patterns.emplace_back( R"(||)", std::vector<std::string>{ "&&", "^^" } );
    patterns.emplace_back( R"(>)", std::vector<std::string>{ ">=", "<", "<=", "==", "!=" } );
    patterns.emplace_back( R"(&)", std::vector<std::string>{ "|", "^" } );
    patterns.emplace_back( R"(|)", std::vector<std::string>{ "&", "^" } );
    patterns.emplace_back( R"(^)", std::vector<std::string>{ "|", "&" } );
  }

public:
  void run()
  {
    load_file();
    generate_mutants();
    // compile_mutants();

    std::cout << "[i] generated " << mutant_counter << " mutants" << std::endl;
    // std::cout << "[i] successfully compiled " << filtered_mutant_counter << " mutants (using yosys)" << std::endl;
    //
    // for ( const auto& id : filtered_mutant_ids )
    // {
    //   std::cout << id << ' ';
    // }
    // std::cout << std::endl;
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
      for ( token const& t : tokenize_line( line, line_number ) )
      {
        for ( auto const& p : patterns )
        {
          /* operator mutations */
          if ( t.lexem == p.first )
          {
            for ( const auto& replace : p.second )
            {
              std::string const new_line =
                line.substr( 0, t.column ) + replace + line.substr( t.column + t.lexem.length() );

              std::cout << source_filename << ":" << t.line << ":" << t.column << ": "
                        << red_color << p.first << reset_color << " ==> "
                        << red_color << replace << reset_color << std::endl;
              std::cout << line << std::endl;
              std::cout << new_line << std::endl;

              write_mutant( "mutant" + std::to_string( mutant_counter ) + ".v", line_number, new_line );
              ++mutant_counter;
            }
          }
        }

        /* signal mutation */
        if ( !( is_keyword( t.lexem ) || is_operator( t.lexem ) ) )
        {
          /* replace signal with bit-wise negation */
          {
            std::string const replace = std::string("( ~(") + t.lexem + ") )";
            std::string const new_line =
              line.substr( 0, t.column ) + replace + line.substr( t.column + t.lexem.length() );

            std::cout << source_filename << ":" << t.line << ":" << t.column << ": "
                      << red_color << t.lexem << reset_color << " ==> "
                      << red_color << replace << reset_color << std::endl;

            // std::cout << line << std::endl;
            // std::cout << new_line << std::endl;

            write_mutant( "mutant" + std::to_string( mutant_counter ) + ".v", line_number, new_line );
            ++mutant_counter;
          }
          /* replace signal with Boolean negation */
          {
            std::string const replace = std::string("( !(") + t.lexem + ") )";
            std::string const new_line =
              line.substr( 0, t.column ) + replace + line.substr( t.column + t.lexem.length() );

            std::cout << source_filename << ":" << t.line << ":" << t.column << ": "
                      << red_color << t.lexem << reset_color << " ==> "
                      << red_color << replace << reset_color << std::endl;

            // std::cout << line << std::endl;
            // std::cout << new_line << std::endl;

            write_mutant( "mutant" + std::to_string( mutant_counter ) + ".v", line_number, new_line );
            ++mutant_counter;
          }
          /* replace with const 0 */
          {
            std::string const replace = "'0";
            std::string const new_line =
              line.substr( 0, t.column ) + replace + line.substr( t.column + t.lexem.length() );

            std::cout << source_filename << ":" << t.line << ":" << t.column << ": "
                      << red_color << t.lexem << reset_color << " ==> "
                      << red_color << replace << reset_color << std::endl;

            // std::cout << line << std::endl;
            // std::cout << new_line << std::endl;

            write_mutant( "mutant" + std::to_string( mutant_counter ) + ".v", line_number, new_line );
            ++mutant_counter;
          }
          /* replace with const 1 */
          {
            std::string const replace = "'1";
            std::string const new_line =
              line.substr( 0, t.column ) + replace + line.substr( t.column + t.lexem.length() );

            std::cout << source_filename << ":" << t.line << ":" << t.column << ": "
                      << red_color << t.lexem << reset_color << " ==> "
                      << red_color << replace << reset_color << std::endl;

            // std::cout << line << std::endl;
            // std::cout << new_line << std::endl;

            write_mutant( "mutant" + std::to_string( mutant_counter ) + ".v", line_number, new_line );
            ++mutant_counter;
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
      int const status = system( command.c_str() );
      if ( status == 0 )
      {
        ++filtered_mutant_counter;
        filtered_mutant_ids.emplace_back( i );
      }
    }
  }

private:
  std::string source_filename;

  std::vector<std::string> lines;
  uint32_t mutant_counter{0};
  uint32_t filtered_mutant_counter{0};
  std::vector<uint32_t> filtered_mutant_ids;

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
