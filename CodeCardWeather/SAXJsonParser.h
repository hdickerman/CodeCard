    /**The MIT License (MIT)
    
    Copyright (c) 2019 by Harold Dickerman
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
    */


////
//// JSON low memory serial parser
////
//// Pass JSON tree to parser a single character at a time by calling 'parse(char);'
////
//// Initialize parser by calling with 'parse(0);'
////
//// return_type is set on type of returned value: RETURN_UNKNOWN (unknown/incomplete), RETURN_KEY (return is key), RETURN_DATA (return is data)
//// return_data is returned value and is either a key value for current level or data for current branch
//// level = level deep in tree.  Tree branch stored in key_value[] char array.
//// The parser ignores all characters before the first '{' is found as well as all characters < 32
//// 
//// The parser only keeps all the keys for a single 'branch' of a JSON tree down to the data element.  The key value for each level 
//// of the branch is stored in the key_value[] array.  The depth of the array as well as the maximim length of the keys and data 
//// must be set in the code.  This design minimizes the amount of memory used by the parser, versus storage of the entire parsed JSON tree.
//// This allows the parser to process extremely large JSON data sets on systems with limited memory.  The downside is that the data must be 
//// handled in a serial fashion as the tree is being parsed)
////
//// When the return_type = RETURN_DATA, the key_value[] for each level of the branch and data element stored in return_value is valid.  
//// The key values can be compared to determine if the desired data element value is required/needed.
////
//// The parser converts all 'escaped' characters (\", \r, \n, \t, \\) to a single value.  It also converts 4 digit unicoded values (e.g. \u201d )
//// to standard ASCII (left and right facing single quote to standard single quote, left and right facing double quote to standard double quote.)
//// See code for other conversions.
////

///JSON definitions
  #define TreeDepth       5 // maximum levels of JSON tree
  #define KeyLength      32 // maximum length of parsed JSON key
  #define ReturnLength  500 // maximum length returned data OR key and must be equal or greater then KeyLength
  
  #define RETURN_UNKNOWN      0  // returned value type is unknown or incomplete
  #define RETURN_KEY          1  // returned value is a key for a branch of the JSON tree
  #define RETURN_DATA         2  // returned value is a data element at the end of a branch of the JSON tree

  // values returned by parser
  char return_value[ReturnLength] = {0};      // buffer maximum length of parsed JSON key or data!!!
  int return_type = RETURN_UNKNOWN;           // value returned from parser
  char key_value[TreeDepth][KeyLength] = {0}; // maximum length of parsed JSON key or data!!!
  int level = 0;                              // number of levels deep in JSON tree (starts at 1)

  // following values are internal to parser, but exposed for access
  bool quote_mode = false;
  bool escape_mode = false;
  bool array_mode = false;
  bool array_end = false;
  bool null_data_ok = false;
  int level_change = 0;
  long charCount = 0; // parsed character count
  long esc_char = 0 ; // value of escaped unicode character 
  int escape_count = 0; // number of characters to process in escape mode, usually 0
///JSON definitions

void parseChar( byte inChar )  // manage individual characters
  {
   
   byte c = inChar;
   ////Serial.println((char)c);

   if (c == 0 ) // reset parser
   {
      level = 0;
      for (int ii = 0; ii < sizeof(return_value); ii++) {return_value[ii] = 0;}  // Clears out old buffer
      ////Serial.print("Starting Level ");
      ////Serial.println(level);
      null_data_ok = false;
      quote_mode = false;
      escape_mode = false;
      array_mode = false;
      array_end = false;
      level_change = 0;
      return_type = RETURN_UNKNOWN;
      charCount = 0;
      esc_char = 0;
      escape_count = 0;
      
      return;
   }

   charCount = charCount + 1;  // count characters through parser

   if (c < 32)  // kill non-printables
   {
       return;
   }

   if (return_type != RETURN_UNKNOWN)  // valid data last pass
   {
      return_type = RETURN_UNKNOWN;  // reset
      for (int ii = 0; ii < sizeof(return_value); ii++) {return_value[ii] = 0;}  // Clears out old buffer
   }

   // process previous level change
   null_data_ok = true;  // null data is allowed, unless leveling down
   if (level_change < 0 ) // process level down delayed one cycle
   {
    level = level + level_change ;
    null_data_ok = false;  // no nulls after level down
    ////Serial.print("Level change ");
    ////Serial.println(level);
   }
   level_change = 0 ;  // reset level change flag
   
   // process previous array change
   if (array_end == true )
   {
    array_mode = false;
    array_end = false;
   }

   if (level == 0)
   {
      if (c == '{' )
      {
        level = 1;
        level_change = 1;
        return;
      }
      else
      {
        return;  //ignore any crap in front until foubd real data
      }
   }

   // Process data

   if (quote_mode == false && strlen(return_value) == 0 && c <= 32)    // remove extra spaces and non-printables
   {
      return;
   }
   
   if (quote_mode == false && strlen(return_value) == 0 && c == '"')  // Quoted sring start? (can only be in beginning....)
   {
      quote_mode = true;
      return;
   }

   if (quote_mode == true && c == '"' && escape_mode == false) // Quoted sring end? (can't be in escape mode)
   {
      quote_mode = false;
      return;
   }

   // Deal with escaped characters
   if (escape_mode == false && c == '\\')  // is there an escape character?
   {
      escape_mode = true;
      escape_count = 0;  // deal with multibyte escape codes
      return;    // throw away current character
   }
   
   if (escape_mode == true)  // escape mode? get next char, translate ascii and end escape mode
   {
      if (escape_count == 0 )  /// assume single char escape code....
      {
        if (c == 'u')  // special unicode case as is multiple characters
        {
          escape_count = 4; // 4 more chars to decode
          esc_char = 0;
          return;  //throw away current character and need to process/translate unicode value before returning character
        }
        else if (c == '"' ) { c = 34; }
        else if (c == '\\') { c = '\\'; }
        else if (c == '\'') { c = '\''; }
        else if (c == 'n') { c = 10; }
        else if (c == 'r') { c = 13; }
        else if (c == 't') { c = 8; }
        escape_mode = false;
      }
      else // escape_count > 0 : deal with multiple byte unicode....
      {
        esc_char = (esc_char * 16) + ((toupper(char(c)) >= 'A') ? toupper(char(c)) - 'A' + 10 : char(c) - '0') ;  // get next char as hex value, multiplying value of previous digit by 16
        escape_count = escape_count - 1 ; // reduce unicode digits left to be processed
        
        if (escape_count > 0)  // more digits to process?
        {
          return; //throw away current character
        }
        else  // escape_count == 0, processed all 4 characterof unicode, do substitution
        {
          if      (esc_char == 8220 ) c = 34 ; //201c "  left double quote
          else if (esc_char == 8221 ) c = 34 ; //201d "  right double quote
          else if (esc_char == 8216 ) c = '\'' ; //2018 ' left single quote
          else if (esc_char == 8217 ) c = '\'' ; //2019 ' right single quote
          else if (esc_char == 8230 ) c = '-' ; //2026 - ... (ellipsis)
          else if (esc_char == 169  ) c = 'c' ; //00A9 c (c) copyright
          else                        c = '~' ; //  Unknown/unable to translate
          esc_char = 0 ; //  clean up
          escape_mode = false;   // end eacape mode
        }
      }
   }

   // Manage opening characters
   if (quote_mode == false && c == '{')
   {
     level = level + 1;   // can do immediately
     level_change = 1;
     ////Serial.print("Level up ");
     ////Serial.println(level);
     return;
   }
 
  if (quote_mode == false && c == '[')
  {
    array_mode = true;
    array_end = false;  // array started, not the end of array
    level = level + 1; // can do immediately
    level_change = 1;
    ////Serial.print("Level up ");
    ////Serial.println(level);
    return;
  }

  // Manage terminators
  if (quote_mode == false && c == ':')  // a ':' means preceeding is a key
  {
    return_type = RETURN_KEY;
  }

  if (quote_mode == false && c == ',')
  {
    return_type = RETURN_DATA;
  }
  
  if (quote_mode == false && c == ']')
  {
    array_end = true;  // clear array mode next pass
    array_mode = false;  // clear array mode next pass
    level_change = - 1; // but do in next pass
    return_type = RETURN_DATA;
    ////Serial.println("Level down ]");
    ////Serial.println(level_change);
  }

  if (quote_mode == false && c == '}')
  {
    level_change = - 1; // but do in next pass
    ////Serial.println("Level down }");
    ////Serial.println(level_change);
    return_type = RETURN_DATA;
  }

  if (return_type != RETURN_UNKNOWN)  // has reached end of string and decided type
  {
   ////Serial.print("return_type ");
   ////Serial.println(return_type);
   ////Serial.print("return_value ");
   ////Serial.println(return_value);
   ////Serial.print("Level change ");
   ////Serial.println(level_change);
   ////Serial.print("Level ");
   ////Serial.println(level);
   ////Serial.println("nullable");
   ////Serial.println(null_data_ok);

   if (null_data_ok == false && strlen(return_value) == 0) {return_type = RETURN_UNKNOWN;} // override return type if return is null and not allowed
   return;     
  }

  // handled all other cases, so if here, must append character if not overflowing buffer
  if (strlen(return_value) < sizeof(return_value))  // prevent overflow
  {
    return_value[strlen(return_value)] = c ; // append to buffer
  }

  return;  // got here, so done....
}

void parse( byte inChar )   // parse and manage returned keys
{
  parseChar(inChar);
  
  // save keys for current level
  if (return_type == RETURN_KEY)  // Was return a key?
  {
    for (int ii = level; ii < TreeDepth; ii++){key_value[ii][0] = 0;}  // clear remaining lower keys
    strcpy(key_value[level],return_value);   // save key based on level
  }

  /*  Uncomment for debugging/development
  if (return_type == RETURN_DATA)  // Was return a key?
  {
    Serial.print(" Level ");
    Serial.print(level);
    Serial.print(" Type ");
    Serial.print(return_type);
    Serial.print(" Array ");
    Serial.print(array_mode);
    Serial.print(" Key >");
    Serial.print(key_value[0]);
    Serial.print(":");
    Serial.print(key_value[1]);
    Serial.print(":");
    Serial.print(key_value[2]);
    Serial.print(":");
    Serial.print(key_value[3]);
    Serial.print(":");
    Serial.print(key_value[4]);
    Serial.print("<");
    Serial.print(" Value ");
    Serial.print(return_value);
    Serial.println();
    Serial.flush();
  }
  //*/
}
