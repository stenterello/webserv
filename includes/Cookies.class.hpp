#pragma once

# include <map>
# include <iostream>

class Cookies
{
	private:
		std::map<std::string, std::string>				_items;
		size_t											_size;
		
	public:
		Cookies() : _size(0) { };
		Cookies(const Cookies & cpy);
		Cookies operator=(const Cookies & rhs) const;
		~Cookies() { _items.clear(); };
		
		// member functions
		// unsigned long	hashFunc(std::string str) {
		// 	unsigned long i = 0;

		// 	for (std::string::iterator it = str.begin(); it != str.end(); it++)
		// 		i += *it;
		// 	return (i % 5000);
		// }

		std::string generateCookie(std::string name) {
			std::string ninjaName[28] = { "ka", "zu", "mi", "te", "ku", "lu","ji", 
    									"ri", "ki", "zu", "me", "ta", "rin", "to", "mo", "no","no", 
    									"ke", "shi", "ari", "chi", "do", "ru", "mei", "na", "fu", "zi" };
			
			std::string	ret;
			for (std::string::iterator it = name.begin(); it != name.end(); it++) {
				ret.append(ninjaName[tolower(*it) - 'a']);
			}
			return (ret);
		}

		void print_table() {
			std::cout << "\nHash Table\n----------------\n";
			for (std::map<std::string, std::string>::iterator it = _items.begin(); it != _items.end(); it++) {
				std::cout << it->first << ": " << it->second << std::endl;
			}
			std::cout << "\n----------------\n";
		}

		void insert(std::string key, std::string value) {
			if (_size < 100)
				_size++;
			else
				_items.erase(_items.begin());
			_items.insert(std::make_pair(key, value));
		}
};