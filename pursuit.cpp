#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <regex>
#include <map>
#include <list>
#include <cmath>
#include <sstream>

const double PI = 3.1415926535897932;
typedef sf::Vector2<double> vec2;

namespace S_re {
	const std::regex property("\\s*([_a-zA-Z0-9]+)\\s*=\\s*(.+?)(?:;.*)?");
	const std::regex empty_line("\\s*(?:;.*)?");
	const std::regex preycontrol("\\s*PreyControl:\\s*?(?:;.*)?");
	const std::regex predator("\\s*Predator:\\s*?(?:;.*)?");
	const std::regex control_value(
		"\\s*(-?\\d+(?:\\.\\d*)?)"
		"\\s*(-?\\d+(?:\\.\\d*)?)"
		"\\s*(\\d+(?:\\.\\d*)?)?"
		"\\s*(?:;.*)?"
	); // u.x, u.y, duration
	const std::regex rotate_value(
		"\\s*rotate"
		"\\s*(-?\\d+(?:\\.\\d*)?d?)"
		"\\s*(\\d+(?:\\.\\d*)?d?)?"
		"\\s*(-?\\d+(?:\\.\\d*)?d?)?" // d?
		"\\s*(?:;.*)?"
	); // rotate, rotation speed, duration, starting rotation

	const std::regex numbers{
		"(-?\\d+(?:\\.\\d*)?)\\s*"
		"(?:,\\s*(-?\\d+(?:\\.\\d*)?)\\s*)?"
		"(?:,\\s*(-?\\d+(?:\\.\\d*)?)\\s*)?"
	};
}

class Simulation : public sf::Drawable {

public:
	struct Predator : sf::CircleShape {
		double lambda = 0;
		vec2 position;
		vec2 velocity;
		sf::Color color {0, 0, 0, 0}; 
		//zero opacity for further default initialization
		sf::VertexArray trail{ sf::PrimitiveType::Lines };
		float when_reached = -1.f;
	};
	
	std::list<Predator> predators;

private:
	struct Movement {
		bool rotating;
		double x; // rotation speed
		double y; // rotation starting direction
		double duration;
		Movement(bool rotating, double x, double y, double duration)
			: rotating(rotating), x(x), y(y), duration(duration) {}
	};

	std::list<Movement> movements;
	std::list<Movement>::iterator current_movement;
	double time_of_next_movement = 0.;

	sf::CircleShape prey;

	vec2 prey_position{ 0., 0. };
	vec2 prey_velocity{ 0., 0. };

	bool trail_on_gap = true;
	bool move_by_plan = true;

	float elapsed_last = 0.f;

	bool valid = false;

	static sf::Vector2f to_vec2f(vec2 v) {
		return sf::Vector2f{ (float)v.x, (float)v.y };
	}

	static double dot_product(vec2 z, vec2 v) {
		return z.x * v.x + z.y * v.y;
	}

	static double distance(vec2 a, vec2 b) {
		vec2 r = a - b;
		return std::sqrt(dot_product(r, r));
	}

	static vec2 normalize(vec2 v, double target_length) {
		if (v == vec2()) return v;
		double len = std::sqrt(dot_product(v, v));
		return vec2(v.x * target_length / len,
			v.y * target_length / len);
	}

	static void align_rotation_to_vec(sf::Transformable& obj, sf::Vector2f vec) {
		if (vec != sf::Vector2f())
			obj.setRotation(std::atan2(vec.x, -vec.y) * 180.f / (float)PI);
	}

	static double alpha(vec2 z, vec2 v, double a) {
		double zz = dot_product(z, z);
		double vv = dot_product(v, v);
		double zv = dot_product(z, v);
		double root = std::sqrt(zv * zv + zz * (a * a - vv));
		return (zv + root) / zz;
	}

	bool predator_close_to_prey(const Predator& predator) {
		return distance(prey_position, predator.position) <
			(predators_speed - prey_speed) * elapsed_last;
	}

	//for file initialization begin

	static bool match_number_count(
		const std::string& str, std::smatch& number_match, int count
	) {
		bool can_be_neg = count < 0;
		if (can_be_neg) count = -count;
		if (!std::regex_match(str, number_match, S_re::numbers))
			return false;
		if (number_match[count + 1].length() || !number_match[count].length())
			return false;
		if (!can_be_neg)
			for (int i = 1; i <= count; ++i)
				if (*number_match[i].str().begin() == '-')
					return false;
		return true;
	}

	bool set_prey_position(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, -2))
			return false;
		prey_position = vec2(std::stod(i_match[1]), std::stod(i_match[2]));
		return true;
	}

	bool set_predator_position(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, -2))
			return false;
		predators.back().position = vec2(std::stod(i_match[1]), std::stod(i_match[2]));
		return true;
	}

	bool set_prey_speed(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 1))
			return false;
		prey_speed = std::stod(i_match[1]);
		return true;
	}

	bool set_predators_speed(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 1))
			return false;
		predators_speed = std::stod(i_match[1]);
		return true;
	}

	bool set_prey_color(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 3))
			return false;
		prey_color = sf::Color(
			std::stoi(i_match[1]), std::stoi(i_match[2]), std::stoi(i_match[3]));
		return true;
	}

	bool set_predator_color(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 3))
			return false;
		predators.back().color = sf::Color(
			std::stoi(i_match[1]), std::stoi(i_match[2]), std::stoi(i_match[3]));
		return true;
	}

	bool set_lambda(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 1))
			return false;
		predators.back().lambda = std::stod(i_match[1]);
		return predators.back().lambda >= 0 && 
			predators.back().lambda <= 1;
	}

	bool set_background_color(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 3))
			return false;
		background_color = sf::Color(
			std::stoi(i_match[1]), std::stoi(i_match[2]), std::stoi(i_match[3]));
		return true;
	}

	bool set_text_color(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 3))
			return false;
		text_color = sf::Color(
			std::stoi(i_match[1]), std::stoi(i_match[2]), std::stoi(i_match[3]));
		return true;
	}

	bool set_character_size(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 1))
			return false;
		character_size = std::stoi(i_match[1]);
		return true;
	}

	bool set_point_radius(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 1))
			return false;
		base_radius = std::stod(i_match[1]);
		return true;
	}

	bool set_trail(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 2))
			return false;
		trail_dash_time = std::stod(i_match[1]);
		trail_gap_time = std::stod(i_match[2]);
		return true;
	}

	bool set_zoom(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 1))
			return false;
		zoom = std::stod(i_match[1]);
		return true;
	}

	bool set_scale_speed(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 1))
			return false;
		scale_speed = std::stod(i_match[1]);
		return true;
	}

	bool set_rotation_acceleration(std::smatch& match) {
		std::smatch i_match;
		const std::string& str = match[2].str();
		if (!match_number_count(str, i_match, 1))
			return false;
		prey_rotation_acceleration = std::stod(i_match[1]);
		return true;
	}

	bool add_straight_control(std::smatch& match) {
		if (!movements.empty() && movements.back().duration == 0.) {
			movements.pop_back();
		}
		double dur = match[3].length() ? std::stod(match[3]) : 0.;
		movements.emplace_back(false,
			std::stod(match[1]), std::stod(match[2]), dur);
		return true;
	}

	bool add_rotating_control(std::smatch& match) {
		if (!movements.empty() && movements.back().duration == 0.) {
			movements.pop_back();
		}
		if (!match[2].str().empty() && *match[2].str().rbegin() == 'd')
			return false;

		double speed = std::stod(match[1]);
		if (*match[1].str().rbegin() == 'd')
			speed = speed * PI / 180.;
		double dur = match[2].length() ? std::stod(match[2]) : 0.;
		double start = match[3].length() ? std::stod(match[3]) : std::nan("");
		if (match[3].length() != 0 && *match[3].str().rbegin() == 'd')
			start = start * PI / 180.;
		if (movements.empty() && !std::isnan(start))
			align_rotation_to_vec(prey,
				sf::Vector2f(std::cos(start), std::sin(start)));

		movements.emplace_back(true,
			speed, start, dur);
		return true;
	}

	vec2 naiveDirection(const Predator& predator) {
		return normalize(prey_position - predator.position, 1);
	}

	vec2 parallelDirection(const Predator& predator) {
		double a = predators_speed / prey_speed;
		vec2 z = (predator.position - prey_position) / prey_speed; // X -- parallel, Y -- prey
		vec2 v = normalize(prey_velocity, prey_speed);
		vec2 u = v - z * alpha(z, v, a);
		return normalize(u, 1);
	}


public:

	float base_radius = 15.f;
	float zoom = 0.02f;
	float scale_speed = 0.003f;
	float prey_rotation_acceleration = 1.f;
	float time_scale = 1.f;
	int substeps = 1;
	double prey_rotation_speed = 1.;
	sf::Vector2f view_center{};

	sf::View view;

	double prey_speed = 1.0; // 1.0
	double predators_speed = 1.2; // 1.5

	sf::Color prey_color{ sf::Color::Blue };
	sf::Color background_color{ 247, 247, 247 };
	sf::Color text_color{ 16, 16, 16 };
	int character_size = 20;

	float trail_dash_time = 0.05f;
	float trail_gap_time = 0.02f;

	sf::VertexArray prey_trail{ sf::PrimitiveType::Lines };

	float simulation_timer = 0.f;

	Simulation() {}
	Simulation(std::istream& file); // declaration because of std::map

	void setPreyPosition(vec2 value) {
		prey_position = value;
		prey.setPosition(to_vec2f(prey_position));
	}

	void setPreyVelocity(vec2 value) {
		move_by_plan = false;
		prey_velocity = normalize(value, prey_speed);
		align_rotation_to_vec(prey, to_vec2f(prey_velocity));
	}

	void rotatePreyVelocity(double rotation) {
		move_by_plan = false;
		double angle = prey_velocity == vec2() ? 0 : std::atan2(prey_velocity.y, prey_velocity.x);
		angle += rotation;
		prey_velocity = normalize(vec2(std::cos(angle), std::sin(angle)), prey_speed);
		align_rotation_to_vec(prey, to_vec2f(prey_velocity));
	}

	vec2 getPreyPosition() { return prey_position; }
	vec2 getPreyVelocity() { return normalize(prey_velocity, prey_speed); }
	vec2 getPredatorPosition(const Predator& predator) { return predator.position; }
	vec2 getPredatorVelocity(const Predator& predator) { return elapsed_last ? predator.velocity : vec2(); }

	bool is_valid() { return valid; }

	void applyZoom() {
		float point_radius = zoom * base_radius;
		prey.setOrigin(point_radius, point_radius);
		prey.setRadius(point_radius);
		for (Predator& predator : predators) {
			predator.setOrigin(point_radius, point_radius);
			predator.setRadius(point_radius);
		}
	}

	void singleStepSimulate(float elapsed) { // substeps??
		if (simulation_timer == 0.f && current_movement->rotating && !std::isnan(current_movement->y)) {
			prey_velocity = normalize(vec2(
				std::cos(current_movement->y), std::sin(current_movement->y)), prey_speed);
		}

		elapsed_last = elapsed;
		
		for (Predator& predator : predators) {
			if (predator.when_reached < 0.f && predator_close_to_prey(predator))
			predator.when_reached = simulation_timer;
		}

		static float trail_timer = 0.f;
		static bool trail_gap_now = true;

		// prey control
		if (move_by_plan) {
			while (simulation_timer >= time_of_next_movement) {
				++current_movement;
				time_of_next_movement += current_movement->duration;
				if (current_movement->rotating && !std::isnan(current_movement->y)) {
					prey_velocity = normalize(vec2(
						std::cos(current_movement->y), std::sin(current_movement->y)), prey_speed);
				}
			}

			if (!current_movement->rotating) {
				prey_velocity = normalize(vec2(
					current_movement->x, current_movement->y), prey_speed);
			}
			else {
				double angle = std::atan2(prey_velocity.y, prey_velocity.x);
				angle += elapsed * current_movement->x;
				prey_velocity = normalize(vec2(
					std::cos(angle), std::sin(angle)), prey_speed);
			}
		}

		for (Predator& predator : predators) {
			if (predator.when_reached < 0.f) {
				vec2 naive_direction = naiveDirection(predator);
				vec2 parallel_direction = parallelDirection(predator);
				vec2 propnav_direction = predator.lambda * parallel_direction + 
					((1 - predator.lambda) * naive_direction);
				vec2 propnav_movement = normalize(propnav_direction, predators_speed * elapsed);
				predator.position += propnav_movement;
				predator.velocity = propnav_movement / double(elapsed);
				align_rotation_to_vec(predator, to_vec2f(propnav_direction));
				predator.setPosition(to_vec2f(predator.position)); // TODO: OY direction
			}
		}

		vec2 prey_movement = normalize(prey_velocity, prey_speed * elapsed);

		prey_position += prey_movement;
		align_rotation_to_vec(prey, to_vec2f(prey_velocity));
		prey.setPosition(to_vec2f(prey_position)); // TODO: OY direction

		trail_timer -= elapsed;
		if (trail_timer < 0.f) {
			prey_trail.append(sf::Vertex(to_vec2f(prey_position), prey_color));
			for (Predator& predator : predators)
				if (predator.when_reached < 0.f)
					predator.trail.append(sf::Vertex(to_vec2f(predator.position), predator.color));
			trail_timer += trail_gap_now ? trail_dash_time : trail_gap_time;
			trail_gap_now = !trail_gap_now;
		}

		simulation_timer += elapsed;
	}

	void simulate(float elapsed) {
		if (substeps <= 0) return;
		elapsed /= substeps;
		for (int i = 0; i < substeps; ++i)
			singleStepSimulate(elapsed);
	}

	virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const {
		target.draw(prey_trail);
		for (const Predator& predator : predators)
			target.draw(predator.trail);

		target.draw(prey);
		for (const Predator& predator : predators)
			target.draw(predator);
	}
};

std::map<std::string, bool(Simulation::*)(std::smatch&)> simulation_setters;
std::map<std::string, bool(Simulation::*)(std::smatch&)> predator_setters;

Simulation::Simulation(std::istream& file) {
	if (simulation_setters.empty()) {
		simulation_setters["PreyPosition"] = &Simulation::set_prey_position;
		simulation_setters["PreySpeed"] = &Simulation::set_prey_speed;
		simulation_setters["PredatorsSpeed"] = &Simulation::set_predators_speed;
		simulation_setters["PreyColor"] = &Simulation::set_prey_color;
		simulation_setters["BackgroundColor"] = &Simulation::set_background_color;
		simulation_setters["TextColor"] = &Simulation::set_text_color;
		simulation_setters["CharacterSize"] = &Simulation::set_character_size;
		simulation_setters["PointRadius"] = &Simulation::set_point_radius;
		simulation_setters["Trail"] = &Simulation::set_trail;
		simulation_setters["ScaleSpeed"] = &Simulation::set_scale_speed;
		simulation_setters["RotationAcceleration"] = &Simulation::set_rotation_acceleration;
		simulation_setters["Zoom"] = &Simulation::set_zoom;
	}
	if (predator_setters.empty()) {
		predator_setters["Position"] = &Simulation::set_predator_position;
		predator_setters["Color"] = &Simulation::set_predator_color;
		predator_setters["Lambda"] = &Simulation::set_lambda;
	}

	std::string line;
	std::smatch match;
	enum class ReadingState {
		started, predator, control
	} state = ReadingState::started;

	bool reading_broken = false;
	int line_num = 0;
	while (std::getline(file, line)) {
		++line_num;
		if (line.back() == '\r') line.pop_back();
		if (state == ReadingState::started) {
			if (std::regex_match(line, match, S_re::property)) {
				try {
					reading_broken = !((this->*simulation_setters.at(match[1].str()))(match));
				}
				catch (std::out_of_range&) {
					reading_broken = true;
				}
				if (reading_broken)
					std::cout << "Can't set property \"" <<
					match[1].str() << "\" to \"" << match[2].str() << "\"\n";
			}
			else if (std::regex_match(line, match, S_re::empty_line)) {
			}
			else if (std::regex_match(line, match, S_re::predator)) {
				state = ReadingState::predator;
				predators.push_back(Predator());
			}
			else if (std::regex_match(line, match, S_re::preycontrol)) {
				state = ReadingState::control;
			}
			else {
				reading_broken = true;
			}
		}
		else if (state == ReadingState::predator) {
			if (std::regex_match(line, match, S_re::property)) {
				try {
					reading_broken = !((this->*predator_setters.at(match[1].str()))(match));
				}
				catch (std::out_of_range&) {
					reading_broken = true;
				}
				if (reading_broken)
					std::cout << "Can't set property \"" <<
					match[1].str() << "\" to \"" << match[2].str() << "\"\n";
			}
			else if (std::regex_match(line, match, S_re::empty_line)) {
			}
			else if (std::regex_match(line, match, S_re::predator)) {
				if (predators.back().color.a == 0) {
						predators.back().color = sf::Color(
							255 * predators.back().lambda,
							255 * (1 - predators.back().lambda),
							0);
				}
				predators.push_back(Predator());
			}
			else if (std::regex_match(line, match, S_re::preycontrol)) {
				state = ReadingState::control;
			}
			else {
				reading_broken = true;
			}
			
		}
		else if (state == ReadingState::control) {
			if (std::regex_match(line, match, S_re::control_value)) {
				reading_broken = !add_straight_control(match);
			}
			else if (std::regex_match(line, match, S_re::rotate_value)) {
				reading_broken = !add_rotating_control(match);
			}
			else if (std::regex_match(line, match, S_re::empty_line)) {
			}
			else {
				reading_broken = true;
			}
		}
	}
	if (reading_broken) {
		std::cout << "Syntax error at line " << line_num <<" : \"" << line << "\"\n";
		valid = false;
	}
	else if (movements.empty()) {
		std::cout << "Cannot start without control" << '\n';
		valid = false;
	}
	else {
		valid = true;

		prey.setPointCount(3);
		for (Predator& predator : predators)
			predator.setPointCount(3);

		prey.setFillColor(prey_color);
		for (Predator& predator : predators) {
			if (predator.color.a == 0) {
				predator.color = sf::Color(
					sf::Uint8(predator.lambda) * 255,
					sf::Uint8(1. - predator.lambda) * 255,
					0); // default predator color
			}
			predator.setFillColor(predator.color);
		}

		prey.setPosition(to_vec2f(prey_position));
		for (Predator& predator : predators) {
			predator.setPosition(to_vec2f(predator.position));
		}

		current_movement = movements.begin();
		if (!current_movement->rotating)
			align_rotation_to_vec(prey,
				sf::Vector2f(current_movement->x, current_movement->y));
		else if (!std::isnan(current_movement->y))
			align_rotation_to_vec(prey,
				sf::Vector2f(std::cos(current_movement->y), std::sin(current_movement->y)));

		movements.rbegin()->duration = HUGE_VAL;
		time_of_next_movement = movements.front().duration;
	}
}

double len(vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

vec2 new_velocity(float elapsed) {
	const float speed = 1.f;
	static float phase = 0.f;
	phase += elapsed * speed;
	return { 4. * std::cos(phase), 4. * std::sin(phase) };
}

void print_usage(const char* progname) {
	std::cout << "Usage:\n" << 
	progname << " -h prints this help\n" <<
	progname << " [-c] [-H [simulation_step]] <file path>\n"
	"-H (for headless) runs application without GUI and prints timings for predators\n"
	"-c (for compact) prints less output (both GUI and headless)\n"
	"<file path> can be '-', in this case stdin is read for configuration" << std::endl; 
}

int main(int argc, const char* argv[]) {
	bool headless = false;
	bool sim_info_compact = false;
	bool file_specified = false;
	float headless_step = 1e-3;
	Simulation S;

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (file_specified || arg == "-h") {
			print_usage(argv[0]);
			return 0;
		}
		else if (arg == "-c") {
			sim_info_compact = true;
		}
		else if (arg == "-H") {
			headless = true;
		}
		else if (arg == "-") {
			S = Simulation(std::cin);
			file_specified = true;
		}
		else if (headless == true) {
			try {
				headless_step = std::stof(arg);
			}
			catch (std::exception& e) {
				std::ifstream file(arg);
				if (!file.is_open()) {
					std::cout << "Can't open file " << arg << "\n";
					return -1;
				}
				S = Simulation(file);
				file_specified = true;
			}	
		}
		else {
			std::ifstream file(arg);
			if (!file.is_open()) {
				std::cout << "Can't open file " << arg << "\n";
				return -1;
			}
			S = Simulation(file);
			file_specified = true;
		}
	}
	if (!file_specified) {
		std::string file_path;
		std::cout << "Enter file name: ";
		std::getline(std::cin, file_path);
		std::ifstream file(file_path);
		if (!file.is_open()) {
			std::cout << "Can't open file " << file_path << "\n";
			return -1;
		}
		S = Simulation(file);
	}

	if (!S.is_valid()) return -1;

	if (headless) {
		while (true) {
			bool done = true;
			for (Simulation::Predator& predator : S.predators) {
				if (predator.when_reached < 0.f) {
					done = false;
					break;
				}
			}
			if (done) break;
			S.simulate(headless_step);
		}
		for (Simulation::Predator& predator : S.predators) {
			if (sim_info_compact) {
				std::cout << predator.lambda << ' ' << predator.when_reached << std::endl;
			}
			else {
				std::cout << "Lambda " << predator.lambda 
					<< " reached at " << predator.when_reached << std::endl;
			}
		}
	}


	else {

		const unsigned int DEF_WIN_X = 1280, DEF_WIN_Y = 720; // default window size
		sf::Font font;
		if (!font.loadFromFile("resources/arial.ttf"))
		{
			std::cout << "Can't open font file";
			return -1;
		}
		sf::Text sim_info;
		sim_info.setFont(font);
		sim_info.setCharacterSize(S.character_size);
		sim_info.setFillColor(S.text_color);

		sf::ContextSettings context_settings;
		context_settings.antialiasingLevel = 8;

		sf::RenderWindow window(sf::VideoMode(DEF_WIN_X, DEF_WIN_Y), "Pursuit",
			sf::Style::Default, context_settings);
		
		sf::View sim_view{sf::Vector2f(), sf::Vector2f(
			DEF_WIN_X * S.zoom, DEF_WIN_Y * S.zoom)};
		sf::View text_view{ sf::FloatRect(0.f, 0.f,
			static_cast<float>(DEF_WIN_X), static_cast<float>(DEF_WIN_Y))};
		sf::View captured_view = sim_view; // view for restore
		//float captured_zoom = S.zoom; // -Wunused-but-set-variable
		//i really don't know why i did it
		int last_mouse_y;
		sf::Vector2f captured_coords;
		sf::Vector2i current_screen_mouse_pos;
		sf::Vector2f current_world_mouse_pos;
		bool RMB_pressed = false;
		bool ctrl_pressed = false;
		bool running = false;

		sf::Clock clock;

		while (window.isOpen()) {
			sf::Event event;
			while (window.pollEvent(event)) {
				switch (event.type) {
				case sf::Event::Closed:
					window.close();
				break;
				case sf::Event::Resized:
				{
					sim_view.setSize(event.size.width * S.zoom, event.size.height * S.zoom);
					text_view = sf::View(sf::FloatRect(0.f, 0.f,
						static_cast<float>(event.size.width), static_cast<float>(event.size.height)));
				}
				break;
				case sf::Event::KeyPressed:
					switch (event.key.code) {
					case sf::Keyboard::Space:
						running = !running;
					break;
					case sf::Keyboard::LControl:
						if (!sf::Mouse::isButtonPressed(sf::Mouse::Right))
							ctrl_pressed = true;
					break;
					case sf::Keyboard::Z:
						if (sf::Keyboard::isKeyPressed(sf::Keyboard::LControl))
							++S.substeps;
						else
							S.time_scale *= 2.f;
					break;
					case sf::Keyboard::X:
						if (sf::Keyboard::isKeyPressed(sf::Keyboard::LControl)) {
							if (S.substeps > 1) --S.substeps;
						}
						else
							S.time_scale *= 0.5f;
					break;
					default: break;
					}
				break;
				case sf::Event::KeyReleased:
					switch (event.key.code) {
					case sf::Keyboard::LControl:
						if (!sf::Mouse::isButtonPressed(sf::Mouse::Right))
							ctrl_pressed = false;
					break;
					default: break;
					}
				break;
				case sf::Event::MouseButtonPressed:
					if (event.mouseButton.button == sf::Mouse::Right) {
						captured_view = sim_view;
						//captured_zoom = S.zoom;
						RMB_pressed = true;
						if (ctrl_pressed) {
							last_mouse_y = event.mouseButton.y;
						}
						else {
							captured_coords = window.mapPixelToCoords(
								sf::Mouse::getPosition(window), captured_view);
						}
					}
				break;
				case sf::Event::MouseButtonReleased:
					if (event.mouseButton.button == sf::Mouse::Right) {
						captured_view = sim_view;
						//captured_zoom = S.zoom;
						RMB_pressed = false;
					}
				break;
				case sf::Event::MouseMoved:
					current_screen_mouse_pos = sf::Vector2i(event.mouseMove.x, event.mouseMove.y);
					current_world_mouse_pos = window.mapPixelToCoords(current_screen_mouse_pos, captured_view);
					
					if (RMB_pressed) {
						if (ctrl_pressed) {
							float ratio = std::exp(S.scale_speed * (
								last_mouse_y - sf::Mouse::getPosition(window).y));
							last_mouse_y = sf::Mouse::getPosition(window).y;
							S.zoom *= ratio;
							sim_view.zoom(ratio);
						}
						else {
							sf::View view = captured_view;
							view.move(captured_coords - window.mapPixelToCoords(
								sf::Mouse::getPosition(window), captured_view));
							sim_view = view;
						}
					}
					break;
				default: break;
				}
			}
	#if 0
			std::cout << window.getView().getCenter().x << ' ' << window.getView().getCenter().y << ' ' <<
				window.getView().getSize().x << ' ' << window.getView().getSize().y << '\n';
	#endif
			float elapsed = clock.restart().asSeconds();
			elapsed *= S.time_scale;

			// not planned prey moves
			// minus should go to right when OY will be directed as needed
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
				S.rotatePreyVelocity(-S.prey_rotation_speed * elapsed);
			}
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
				S.rotatePreyVelocity(S.prey_rotation_speed * elapsed);
			}
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
				S.prey_rotation_speed *= std::exp(S.prey_rotation_acceleration * elapsed);
			}
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
				S.prey_rotation_speed *= std::exp(-S.prey_rotation_acceleration * elapsed);
			}

			if (running) {
				S.simulate(elapsed);
			}
			S.applyZoom();
			
			std::stringstream ss_sim_info;
			ss_sim_info << "Timer: " << S.simulation_timer
				<< "\nFPS: " << 1.f/elapsed
				<< "\nTime scale: " << S.time_scale
				<< "\nSimulation substeps: " << S.substeps
<< "\nMouse position: (" << current_world_mouse_pos.x << ", " << -current_world_mouse_pos.y 
<< ")\nPrey position: (" << S.getPreyPosition().x << ", " << -S.getPreyPosition().y
<< ")\nPrey velocity: (" << S.getPreyVelocity().x << ", " << -S.getPreyVelocity().y
<< ")\nPrey speed: " << len(S.getPreyVelocity());
			int predator_num = 0;
			for (Simulation::Predator& predator : S.predators) {
				++predator_num;
				if (sim_info_compact) {
					ss_sim_info << "\nPredator " << predator.lambda 
<< " (" << predator.position.x << ", " << -predator.position.y << ") ";
					if (predator.when_reached < 0.f) {
						ss_sim_info << "(" << S.getPredatorVelocity(predator).x 
							<< ", " << -S.getPredatorVelocity(predator).y << ")";
					}
					else {
						ss_sim_info << "[" << predator.when_reached << "]";
					}
				}
				else {
ss_sim_info << "\nPredator " << predator_num
<< ":\n|||Lambda: " << predator.lambda
<< "\n|||Position: (" << predator.position.x << ", " << -predator.position.y
<< ")\n|||Velocity: (" << S.getPredatorVelocity(predator).x << ", " << -S.getPredatorVelocity(predator).y
<< ")\n|||Speed: " << len(S.getPredatorVelocity(predator))
<< "\n|||When reached:" << predator.when_reached;
				}
			}

			sim_info.setString(ss_sim_info.str().c_str());
			window.clear(S.background_color);
			window.setView(sim_view);
			window.draw(S);
			window.setView(text_view);
			window.draw(sim_info);
			window.display();
		}
	}
	return 0;
}
