#include "libtcod.hpp"
#include "SDL.h"
#include <string>
#include <sstream>
#include <cmath>

using namespace std;

// konstanty pro zobrazování
const int SCREEN_WIDTH = 80;
const int SCREEN_HEIGHT = 50;
const int FPS_LIMIT = 20;

// konstanty pro velikost a generování bludištì
const int MAP_WIDTH = 80;
const int MAP_HEIGHT = 45;

const int ROOM_MAX_SIZE = 10;
const int ROOM_MIN_SIZE = 6;
const int MAX_ROOMS = 30;

const int MAX_ROOM_MONSTERS = 3;
const int MAX_ROOM_ITEMS = 2;

// konstanty pro uživatelské rozhraní
const int BAR_WIDTH = 20;
const int PANEL_HEIGHT = 7;
const int PANEL_Y = SCREEN_HEIGHT - PANEL_HEIGHT;

const int MSG_X = BAR_WIDTH + 2;
const int MSG_WIDTH = SCREEN_WIDTH - BAR_WIDTH - 1;
const int MSG_HEIGHT = PANEL_HEIGHT - 1;

const int INVENTORY_WIDTH = 50;

// konstanty pro zobrazování bludištì
TCODColor color_dark_ground(0,0,50);
TCODColor color_light_ground(0,0,100);
TCODColor color_dark_wall(50,50,100);
TCODColor color_light_wall(50,50,150);

const TCOD_fov_algorithm_t FOV_ALGO = FOV_BASIC;
const bool FOV_LIGHT_WALLS = true;
const int TORCH_RADIUS = 10;

// konstanty pro pøedmìty
const int HEAL_AMOUNT = 4;
const int LIGHTNING_RANGE = 5;
const int LIGHTNING_DAMAGE = 20;
const int FIREBALL_RADIUS = 3;
const int FIREBALL_DAMAGE = 12;
const int CONFUSE_RANGE = 8;
const int CONFUSE_NUM_TURNS = 10;

// deklarace nejdùležitìjších tøíd
class Object;
class Fighter;
class AI;
class Item;

// string + barva
struct Message {
	string str;
	TCODColor col;
	bool operator==(const Message &m); // operátor== vyžadovaný tøídou TCODList
};

bool Message::operator==(const Message &m) {
	return (str == m.str && col == m.col);
}

// entita v bludišti
class Object {
public:
	char chr; // znak pro zobrazení
	TCODColor color; // barva
	int x,y; // poloha
	string name; // jméno
	bool blocks; // znemožòuje prùchod?
	Fighter *fighter; // komponenty
	AI *ai;
	Item *item;
	Object(int x_, int y_, char chr_, string name_, const TCODColor & color_, bool blocks_=false,
		Fighter *fighter_component=0, AI *ai_component=0, Item *item_component=0);
	void move(int dx, int dy); // popojít o (dx,dy)
	void move_towards(int tgt_x, int tgt_y); // popojít smìrem k (tgt_x,tgt_y)
	void send_to_back(); // poslat na pozadí
	float distance_to(Object &other); // vzdálenost k jinému objektu
	float distance_to(int tgt_x, int tgt_y); // vzdálenost k (tgt_x,tgt_y)
	void draw(); // vykreslení
};

// komponenta pro entity, které mùžou bojovat
class Fighter {
public:
	Object* owner; // odkaz na vlastníka
	void (*death)(Object&); // funkce spuštìná pøi smrti
	int hp, max_hp, def, pow; // vlastnosti
	Fighter(int hp, int def, int pow, void (*death_function)(Object&) = 0):
		hp(hp), max_hp(hp), def(def), pow(pow), death(death_function) {;}
	void take_damage(int dmg); // obdržení damage
	void attack(Object &target); // útok na entitu target
	void heal(int amount); // ozdravení
};

// komponenta pro entity, které samy pøemýšlí
// abstraktní tøída
class AI {
public:
	Object* owner; // odkaz na vlastníka
	virtual void take_turn() = 0;
	virtual string save() = 0;
};

// základní pøíšerka, jen jde za hráèem
class BasicMonster: public AI {
public:
	virtual void take_turn();
	virtual string save();
};

// zmatená pøíšerka, chodí náhodnì
class ConfusedMonster: public AI {
public:
	AI *old_ai; // AI, které pøíšerka mìla pùvodnì
	int num_turns; // poèet kol, po které pøíšerka ještì bude zmatená
	ConfusedMonster(AI *old_ai, int num_turns = CONFUSE_NUM_TURNS): old_ai(old_ai), num_turns(num_turns) {}
	virtual void take_turn();
	virtual string save();
};

// komponenta pro entity, které lze pøidat do inventáøe
class Item {
public:
	Object *owner; // odkaz na vlastníka
	int (*use_function)(); // samotná funkce pøedmìtu
	Item(int (*use_function)() = 0): use_function(use_function) {}
	void pick_up(); // pøidání pøedmìtu z bludištì do inventáøe
	void use(); // použití pøedmìtu
	void drop(); // vrácení pøedmìtu z inventáøe do bludištì
};

// políèko v bludišti
class Tile {
public:
	bool blocked, blocks_sight, explored; // brání v prùchodu; není prùhledné; hráè jej už vidìl
	Tile(bool blocked = true): blocked(blocked), blocks_sight(blocked), explored(false) {;}
	Tile(bool blocked, bool blocks_sight): blocked(blocked), blocks_sight(blocks_sight), explored(false) {;}
};

// pomocná tøída pro obdélník
class Rect {
public:
	int x1,x2,y1,y2,center_x,center_y;
	Rect(int x = 0, int y = 0, int w = 0, int h = 0):
		x1(x), y1(y), x2(x+w), y2(y+h), center_x(x + w/2), center_y(y + h/2) {;}
	bool intersect(Rect &other); // protíná obdélník nìjaký jiný?
};

bool isBlocked(int x, int y); // je na políèku (x,y) nìco, co brání v prùchodu?
string numToStr(int); // pomocné funkce pro konverzi èísel na string
string numToStr(float);
string numToStr(double);
void target_tile(int &tgt_x, int &tgt_y, int max_range = -1); // GUI: vybrat políèko
Object* target_monster(int max_range = -1); // GUI: vybrat pøíšerku
void message(string new_msg,const TCODColor& color = TCODColor::white); // GUI: dát zprávu do msgboxu
string capitalize(string &s); // pomocná funkce pro velké první písmeno
float round(float _x); // pomocné funkce pro zaokrouhlování
double round(double _x);
void render_all(); // vykreslení všeho na obrazovku

Tile** map; // bludištì
Object* player; // hráè
TCODList<Object*> inventory; // inventáø
TCODMap fov_map(MAP_WIDTH,MAP_HEIGHT); // dat. struktura pro field of view
TCODList<Object*> objects; // entity v bludišti
TCODList<Message> game_msgs; // GUI: msgbox
TCODConsole *con = new TCODConsole(SCREEN_WIDTH,SCREEN_HEIGHT); // bludištì
TCODConsole *panel = new TCODConsole(SCREEN_WIDTH,PANEL_HEIGHT); // informaèní panel
TCODRandom *rng = TCODRandom::getInstance(); // generátor náh. èísel
string game_state = "playing"; // stav hry
string player_action = " "; // hráèova akce
bool fov_recompute = true; // je potøeba pøepoèítat field of view?
int total_monsters; // celkový poèet pøíšerek v bludišti

Object::Object(int x_, int y_, char chr_, string name_, const TCODColor &color_, bool blocks_,
			   Fighter* fighter_component, AI* ai_component, Item* item_component) {
	x = x_;
	y = y_;
	chr = chr_;
	name = name_;
	color = color_;
	blocks = blocks_;
	fighter = fighter_component;
	ai = ai_component;
	item = item_component;
	if (fighter != 0) fighter->owner = this; // nastavení vlastníka komponent
	if (ai != 0) ai->owner = this;
	if (item != 0) item->owner = this;
}

// popojít o (dx,dy)
void Object::move(int dx, int dy) {
	if (!isBlocked(x+dx,y+dy)) { // lze-li projít do cíle
		x += dx;
		y += dy;
	}
}

// popojít k políèku (tgt_x,tgt_y)
void Object::move_towards(int tgt_x, int tgt_y) {
	int dx = tgt_x - x; // (dx,dy) smìrový vektor k cíli
	int dy = tgt_y - y;
	float distance = sqrtf(dx*dx + dy*dy);
	dx = int(round(float(dx)/distance)); // normalizace vektoru
	dy = int(round(float(dy)/distance));
	move(dx,dy);
}

// spoète vzdálenost k entitì
float Object::distance_to(Object &other) {
	int dx = other.x - x;
	int dy = other.y - y;
	return sqrtf(dx*dx + dy*dy);
}

// spoète vzdálenost k políèku
float Object::distance_to(int tgt_x, int tgt_y) {
	int dx = tgt_x - x;
	int dy = tgt_y - y;
	return sqrtf(dx*dx + dy*dy);
}

// pošle entitu na zaèátek vykreslovacího seznamu
// (bude vykreslená první -> bude na pozadí)
void Object::send_to_back() {
	objects.remove(this);
	objects.insertBefore(this,0);
}

// vykreslení
void Object::draw() {
	if (fov_map.isInFov(x,y)) { // je-li entita viditelná
		con->setFore(x,y,color);
		con->setChar(x,y,int(chr));
	}
}

bool Rect::intersect(Rect &other) {
	return (x1 <= other.x2 && x2 >= other.x1 && y1 <= other.y2 && y2 >= other.y1);
}

// zranìní bojovníka
void Fighter::take_damage(int dmg) {
	if (dmg > 0) hp -= dmg;
	if (hp <= 0 && death != 0) death(*owner); // je-li hp < 0, spustit funkci pro úmrtí
}

// zaútoèit na entitu
void Fighter::attack(Object &target) {
	int damage = pow - target.fighter->def; // spoèíst dmg
	if (damage > 0) { // je-li vùbec nìjaké
		message(capitalize(owner->name) + " attacks " + target.name + " for " + numToStr(damage) + " hit points.");
		target.fighter->take_damage(damage); // zranit entitu
	} else { // jinak
		message(capitalize(owner->name) + " attacks " + target.name + " but it has no effect.");
	}
}

// vyléèit
void Fighter::heal(int amount) {
	hp += amount;
	if (hp > max_hp) hp = max_hp; // nelze vyléèit nad max_hp
}

// základní AI pøíšerky
void BasicMonster::take_turn() {
	if (fov_map.isInFov(owner->x, owner->y)) { // je-li pøíšerka viditelná hráèem (<==> pøíšerka vidí hráèe)
		if (owner->distance_to(*player) >= 2.0) { // je-li dál než 2
			owner->move_towards(player->x, player->y); // jít k hráèi
		} else if(player->fighter->hp > 0) {
			owner->fighter->attack(*player); // jinak zaútoèit na hráèe
		}
	}
}

// výpis
string BasicMonster::save() {
	return "b";
}

// AI zmatené pøíšerky
void ConfusedMonster::take_turn() {
	if (num_turns > 0) { // zbývá-li nìjaké zmatení
		owner->move(rng->getInt(-1,1),rng->getInt(-1,1)); // náhodnì popojít
		num_turns -= 1; // posunout poèítadlo
	} else { // jinak
		owner->ai = old_ai; // vrátit pùvodní AI
		message("The " + owner->name + " is no longer confused!", TCODColor::red);
		delete this; // tohle už nepotøebujeme
	}
}

// výpis
string ConfusedMonster::save() {
	return "c" + numToStr(num_turns) + old_ai->save(); // zapíše "c", poèet zbývajících kol a vypíše old_ai
}

// sebrat pøedmìt
void Item::pick_up() {
	if (inventory.size() >= 26) { // nelze mít více než 26 pøedmìtù v inventáøi
		message("Your inventory is full, cannot pick up " + owner->name + ".", TCODColor::red);
	} else {
		inventory.push(owner); // pøidat pøedmìt do inventáøe
		objects.remove(owner); // odebrat pøedmìt z bludištì
		message("You picked up a " + owner->name + "!", TCODColor::green);
	}
}

// použít pøedmìt
void Item::use() {
	if (use_function == 0) { // pokud pøedmìt nelze použít
		message("The " + owner->name + " cannot be used.");
	} else { // jinak
		if (use_function() != -1) { // pokud použití dopadlo úspìšnì
			inventory.remove(owner); // odebrat pøedmìt z inventáøe
		}
	}
}

// zahodit pøedmìt
void Item::drop() {
	objects.push(owner); // pøidat pøedmìt do bludištì
	inventory.remove(owner); // odebrat pøedmìt z inventáøe
	owner->x = player->x; // pøesunout pøedmìt pod hráèe
	owner->y = player->y;
	message("You dropped a " + owner->name + ".",TCODColor::yellow);
}

// pomocné funkce
string numToStr(int i) {
	std::stringstream out;
	out << i;
	return out.str();
}

string numToStr(float f) {
	std::stringstream out;
	out << f;
	return out.str();
}

string numToStr(double d) {
	std::stringstream out;
	out << d;
	return out.str();
}

string capitalize(string &s) {
	string r = s;
	if (r[0] <= 122 && r[0] >= 97) r[0] -= 32;
	return r;
}

float round(float _x) {
	return floor(_x + 0.5);
}

double round(double _x) {
	return floor(_x + 0.5);
}

// brání na políèku (x,y) nìco v prùchodu?
bool isBlocked(int x, int y) {
	if (map[x][y].blocked) return true; // v prùchodu brání bludištì
	for (Object** it = objects.begin(); it < objects.end(); it++) {
		if ((*it)->blocks && (*it)->x == x && (*it)->y == y) return true; // v prùchodu brání entita
	}
	return false; // v prùchodu nebrání nic
}

// vrátí nejbližší viditelnou pøíšerku bližší než je range
Object* closest_monster(int range) {
	Object* closest_enemy = 0; // nejbližší zatím nalezená pøíšerka
	int closest_dist = range + 1; // její vzdálenost k hráèi

	for (Object **it = objects.begin(); it < objects.end(); it++) { // pro všechny viditelné pøíšerky
		if((*it)->fighter != 0 && (*it) != player && fov_map.isInFov((*it)->x,(*it)->y)) {
			float dist = player->distance_to(**it);
			if (dist < closest_dist) { // je-li pøíšerka bližší než zatím nejbližší nalezená
				closest_enemy = (*it); // je pøíšerka nová nejbližší nalezená
				closest_dist = dist;
			}
		}
	}
	return closest_enemy;
}

// zalamování øádkù
TCODList<Message> text_wrap(Message &m, int width) {
	TCODList<Message> ret;
	size_t found = 0;
	while (1) {
		if (m.str.length() < width) { // zbytek se vejde do øádku
			ret.push(m);
			break;
		}
		found = m.str.find_last_of(" ",width); // najít místo zalomení
		if (found == string::npos) found = width;
		Message new_m = {m.str.substr(0,found),m.col}; // zalomit
		ret.push(new_m);
		m.str = m.str.substr(found+1); // se zbytkem udìlat totéž
	}
	return ret;
}

// GUI: vybrat viditelné políèko bližší než max_range (souøadnice se zapíší do tgt_x a tgt_y)
// max_range == -1 -> max_range je nekoneèno
void target_tile(int &tgt_x, int &tgt_y, int max_range) {
	while(1) {
		render_all(); // vykreslíme všechno (tj. odstraníme inventáø)
		TCODConsole::flush();

		TCOD_key_t key = TCODConsole::checkForKeypress();
		TCOD_mouse_t mouse = TCODMouse::getStatus();
		if (key.vk == TCODK_ENTER && key.lalt) { // alt+enter = pøepnout fullscreen
			TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
		}
		if (mouse.lbutton_pressed && fov_map.isInFov(mouse.cx,mouse.cy) &&
			(max_range == -1 || player->distance_to(mouse.cx,mouse.cy) < max_range)) {
			// hráè kliknul na viditelné políèko, které je blíže než max_range
			tgt_x = mouse.cx; // vrátit souøadnice políèka
			tgt_y = mouse.cy;
			return;
		}
		if (mouse.rbutton_pressed || key.vk == TCODK_ESCAPE) { // zrušit výbìr
			tgt_x = -1; // vrátit neplatnou hodnotu
			tgt_y = -1;
			return;
		}
	}
}

// GUI: vybrat viditelnou pøíšerku bližší než max_range
// max_range == -1 -> max_range je nekoneèno
Object* target_monster(int max_range) {
	while(1) {
		int x,y;
		target_tile(x,y,max_range);
		if (x == -1) { // hráè zrušil výbìr
			return 0;
		}
		for (Object **it = objects.begin(); it < objects.end(); it++) {
			if((*it)->x == x && (*it)->y == y && (*it)->fighter != 0 && (*it) != player) {
				// pokud hráè kliknul na pøíšerku, vrátíme ji
				return (*it);
			}
		}
	}
}

// GUI: vypíše zprávu do msgboxu
void message(string new_msg, const TCODColor &color) {
	Message m = {new_msg,color};
	game_msgs.addAll(text_wrap(m,MSG_WIDTH)); // pøidat zalomenou zprávu do seznamu
	while(game_msgs.size() > MSG_HEIGHT) { // pokud je seznam moc dlouhý, zkrátit
		game_msgs.remove(game_msgs.get(0));
	}
}

// GUI: zobrazí menu s hlavièkou header, možnostmi options a šíøkou width
int menu(string header, TCODList<string> &options, int width) {
	int header_height = con->getHeightLeftRect(0,0,width,0,header.c_str()); // poèet øádkù hlavièky
	if (header == "") header_height = 0;
	int height = options.size() + header_height + 2; // výška menu
	TCODConsole *window = new TCODConsole(width,height); // pomocná konzole pro menu
	window->setForegroundColor(TCODColor::white);
	window->printLeftRect(0,0,width,0,TCOD_BKGND_NONE,header.c_str());
	int y = header_height + 1;
	char letter_index = 'a'; // zaèneme u 'a'
	for (string* it = options.begin(); it < options.end(); it++) { // vypíše všechny možnosti
		string letter(1,letter_index);
		string text = "(" + letter + ") " + (*it); // vèetnì písmenka, které je potøeba stisknout
		window->printLeft(0,y,TCOD_BKGND_NONE,text.c_str());
		y += 1;
		letter_index += 1;
	}
	int x = SCREEN_WIDTH/2 - width/2;
	y = SCREEN_HEIGHT/2 - height/2;
	TCODConsole::blit(window, 0, 0, width, height, TCODConsole::root, x, y, 1.0, 0.7); // vykreslí menu
	TCODConsole::root->flush();
	TCOD_key_t key = TCODConsole::waitForKeypress(true); // èeká na volbu
	if (key.vk == TCODK_ENTER && key.lalt) { // alt+enter = pøepnout fullscreen
		TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
	}
	int index = int(key.c - 'a');
	if (index >= 0 && index < options.size()) return index; // je-li volba platná, vrátí ji
	return -1; // jinak vrátí -1
}

// GUI: zobrazí zprávu
void msg_box(string text, int width) {
	TCODList<string> o;
	menu(text, o, width);
}

// vypíše inventáø a vrátí hráèem vybraný pøedmìt
Item* inventory_menu(string header) {
	TCODList<string> options;
	if (inventory.size() == 0) { // je-li inventáø prázdný
		options.push("Inventory is empty."); // upozorní hráèe
	} else { // jinak
		for (Object **it = inventory.begin(); it < inventory.end(); it++) {
			options.push((*it)->name); // vypíše inventáø
		}
	}
	int index = menu(header, options, INVENTORY_WIDTH); // nechá hráèe vybrat
	if (index == -1 || inventory.size() == 0) return 0; // je-li volba neplatná, nevrátí žádný pøedmìt
	return inventory.get(index)->item; // jinak vrátí vybraný pøedmìt
}

// funkce pro uzdravovací lektvar
int cast_heal() {
	if (player->fighter->hp == player->fighter->max_hp) { // je-li hráè zdravý
		message("You are already at full health.", TCODColor::red);
		return -1; // lektvar nebyl použit
	}
	message("Your wounds start to feel better.", TCODColor::violet);
	player->fighter->heal(HEAL_AMOUNT); // uzdraví hráèe
	return 1; // vrátí úspìch
}

// funkce pro svitek blesku
int cast_lightning() {
	Object* monster = closest_monster(LIGHTNING_RANGE); // zamíøí na nejbližší pøíšerku v dosahu
	if (monster == 0) { // pokud není žádná v dosahu
		message("No monster is close enough to strike.", TCODColor::red);
		return -1; // svitek nebyl použit
	}
	message("A lightning bolt strikes the " + monster->name + " for " + numToStr(LIGHTNING_DAMAGE) + " hit points.",TCODColor::lightBlue);
	monster->fighter->take_damage(LIGHTNING_DAMAGE); // poraní pøíšerku
	return 1; // vrátí úspìch
}

// funkce pro svitek ohnivé koule
int cast_fireball() {
	message("Left-click a target tile for the fireball, or right-click to cancel.", TCODColor::lightCyan);
	int x, y;
	target_tile(x,y); // nechá hráèe vybrat cíl
	if (x == -1) { // pokud nebyl vybrán platný cíl
		message("Spell cancelled.", TCODColor::cyan);
		return -1; // svitek nebyl použit
	}
	message("The fireball explodes, burning everything within " + numToStr(FIREBALL_RADIUS) + " tiles!", TCODColor::orange);

	for (Object **it = objects.begin(); it < objects.end(); it++) { // poraní všechny pøíšerky (a hráèe) v dosahu
		if ((*it)->distance_to(x,y) <= FIREBALL_RADIUS && (*it)->fighter != 0) {
			message("The " + (*it)->name + " gets burned for " + numToStr(FIREBALL_DAMAGE) + " hit points.", TCODColor::orange);
			(*it)->fighter->take_damage(FIREBALL_DAMAGE);
		}
	}
	return 1;
}

// funkce pro svitek omámení
int cast_confuse() {
	message("Left-click an enemy to confuse it, or right-click to cancel.", TCODColor::lightCyan);
	Object *monster = target_monster(CONFUSE_RANGE); // nechá hráèe vybrat cíl
	if (monster == 0) { // pokud nebyl vybrán cíl
		message("Spell cancelled.", TCODColor::cyan);
		return -1; // svitek nebyl použit
	}

	AI* old_ai = monster->ai;
	monster->ai = new ConfusedMonster(old_ai); // AI pøíšerky se nahradí zmateným AI
	monster->ai->owner = monster;
	message("The eyes of the " + monster->name + " glaze over, as it starts to stumble around!", TCODColor::orange);
	return 1;
}

// vrátí jména entit pod kurzorem, oddìlená èárkami a s prvním písmenem velkým
string get_names_under_mouse() {
	TCOD_mouse_t mouse = TCODMouse::getStatus();
	string ret;
	for (Object **it = objects.begin(); it < objects.end(); it++) {
		// pøidá do stringu ret jména všech entit pod kurzorem
		if ((*it)->x == mouse.cx && (*it)->y == mouse.cy && fov_map.isInFov((*it)->x,(*it)->y)) {
			ret += (*it)->name + ", ";
		}
	}
	if (ret.length() > 0) ret.erase(ret.end()-2,ret.end()); // umažeme poslední èárku
	return capitalize(ret);
}

// funkce pro smrt hráèe
void player_death(Object& plr) {
	game_state = "dead"; // nastaví stav hry
	plr.chr = '%'; // promìní hráèe v mrtvolu
	plr.color = TCODColor::darkRed;
}

// funkce pro smrt pøíšerky
void monster_death(Object& monster) {
	monster.chr = '%'; // promìní pøíšerku v mrtvolu
	monster.color = TCODColor::darkRed;
	monster.blocks = false; // pøes mrtvolu lze procházet
	delete monster.fighter; // s mrtvolou nelze bojovat
	monster.fighter = 0;
	delete monster.ai; // mrtvola nemyslí
	monster.ai = 0;
	message(capitalize(monster.name) + " is dead!",TCODColor::orange);
	monster.name = "remains of " + monster.name; // zmìní jméno
	monster.send_to_back(); // vrátí na konec
	total_monsters -= 1;
}

// funkce pro pohyb/útok hráèe
void player_move_or_attack(int dx, int dy) {
	int x = player->x + dx;
	int y = player->y + dy;

	Object* target = 0;
	for (Object ** it = objects.begin(); it < objects.end(); it++) {
		// vrazil hráè do pøíšerky?
		if ((*it)->fighter != 0 && (*it)->x == x && (*it)->y == y) {
			target = (*it);
			break;
		}
	}

	if (target != 0) { // pokud ano, tak na ni zaútoèil
		player_action = "attack";
		player->fighter->attack(*target);
	} else { // jinak se jen pohnul
		player->move(dx,dy);
		player_action = "move";
		fov_recompute = true; // pak je potøeba pøepoèítat FoV
	}
}

// vygeneruje místnost v bludišti
void create_room(Rect &room) {
	for (int i = room.x1 + 1; i < room.x2; i++) {
		for (int j = room.y1 + 1; j < room.y2; j++) {
			map[i][j].blocked = false;
			map[i][j].blocks_sight = false;
		}
	}
}

// vygeneruje horizontální tunel v bludišti
void create_h_tunnel(int x1, int x2, int y) {
	int start = x1, end = x2;
	if (start > end) {
		start = x2;
		end = x1;
	}
	for (int i = start; i <= end; i++) {
		map[i][y].blocked = false;
		map[i][y].blocks_sight = false;
	}
}

// vygeneruje vertikální tunel v bludišti
void create_v_tunnel(int y1, int y2, int x) {
	int start = y1, end = y2;
	if (start > end) {
		start = y2;
		end = y1;
	}
	for (int i = start; i <= end; i++) {
		map[x][i].blocked = false;
		map[x][i].blocks_sight = false;
	}
}

// zabydlí místnost
void place_objects(Rect &room) {
	// vygeneruje pøíšerky
	int num_monsters = rng->getInt(0,MAX_ROOM_MONSTERS);
	for (int i = 0; i < num_monsters; i++) {
		int x = rng->getInt(room.x1 + 1,room.x2 - 1); // umístìní
		int y = rng->getInt(room.y1 + 1,room.y2 - 1);
		int choice = rng->getInt(0,100); // výbìr typu
		Object* monster;
		if (choice < 80) {
			monster = new Object(x,y,'o',"orc",TCODColor::desaturatedGreen,true,
				new Fighter(10,0,3,monster_death), new BasicMonster());
		} else {
			monster = new Object(x,y,'T',"troll",TCODColor::darkerGreen,true,
				new Fighter(16,1,4,monster_death), new BasicMonster());
		}
		objects.push(monster);
		total_monsters += 1;
	}
	// vygeneruje pøedmìty
	int num_items = rng->getInt(0,MAX_ROOM_ITEMS);
	for (int i = 0; i < num_items; i++) {
		int x = rng->getInt(room.x1 + 1,room.x2 - 1);
		int y = rng->getInt(room.y1 + 1,room.y2 - 1);
		if (!isBlocked(x,y)) { // je-li místo volné
			int choice = rng->getInt(0,100); // výbìr typu
			Object *item;
			if (choice < 70) {
				item = new Object(x,y,'!',"healing potion",TCODColor::violet,false,0,0,new Item(cast_heal));
			} else if (choice < 80) {
				item = new Object(x,y,'#',"scroll of lightning bolt",TCODColor::lightYellow,false,0,0,new Item(cast_lightning));
			} else if (choice < 90) {
				item = new Object(x,y,'#',"scroll of confusion",TCODColor::lightYellow,false,0,0,new Item(cast_confuse));
			} else {
				item = new Object(x,y,'#',"scroll of fireball",TCODColor::lightYellow,false,0,0,new Item(cast_fireball));
			}
			objects.push(item);
			item->send_to_back();
		}
	}
}

void initialize_fov() {
	for (int i = 0; i < MAP_WIDTH; i++) { // vygenerování mapy pro FoV
		for (int j = 0; j < MAP_HEIGHT; j++) {
			fov_map.setProperties(i,j,!map[i][j].blocked,!map[i][j].blocks_sight);
		}
	}
}

// vygeneruje bludištì
void make_map() {
	player = new Object(0, 0, '@', "player", TCODColor::white, true, new Fighter(30,2,5,player_death));
	objects.push(player);

	map = new Tile*[MAP_WIDTH]; // napøed je vše prázdné
	for (int i = 0; i < MAP_WIDTH; i++) {
		map[i] = new Tile[MAP_HEIGHT];
	}

	TCODList<Rect> rooms;
	int num_rooms = 0;

	// vygeneruje místnosti
	for (int i = 0; i < MAX_ROOMS; i++) {
		int w = rng->getInt(ROOM_MIN_SIZE, ROOM_MAX_SIZE);
		int h = rng->getInt(ROOM_MIN_SIZE, ROOM_MAX_SIZE);
		int x = rng->getInt(0, MAP_WIDTH - w - 2);
		int y = rng->getInt(0, MAP_HEIGHT - h - 2);
		Rect new_room(x,y,w,h);
		bool failed = false; // protíná se nová místnost s už nìjakou vygenerovanou?
		for (Rect* it = rooms.begin(); it < rooms.end(); it++) {
			if (new_room.intersect(*it)) {
				failed = true;
				break;
			}
		}
		if (!failed) { // pokud ne, pøidá se do mapy
			create_room(new_room);
			int new_x = new_room.center_x;
			int new_y = new_room.center_y;
			if (num_rooms == 0) { // je-li to první místnost, bude v ní hráè
				player->x = new_x;
				player->y = new_y;
			} else { // jinak se spojí s pøedchozí vygenerovanou místností tunely
				int prev_x = rooms.get(num_rooms-1).center_x;
				int prev_y = rooms.get(num_rooms-1).center_y;
				if (rng->getInt(0,1)) {
					create_h_tunnel(prev_x,new_x,prev_y);
					create_v_tunnel(prev_y,new_y,new_x);
				} else {
					create_v_tunnel(prev_y,new_y,prev_x);
					create_h_tunnel(prev_x,new_x,new_y);
				}
			}
			rooms.push(new_room);
			place_objects(new_room); // zabydlí novou místnost
			num_rooms += 1;
		}
	}
}

// GUI: vykreslí ukazatel hodnoty na souøadnicích (x,y) o daných barvách
void render_bar(int x, int y, int total_width, string name, int value, int maximum,
				const TCODColor &bar_color, const TCODColor &back_color) {
	int bar_width = int(float(total_width*value)/maximum); // pozadí
	panel->setBackgroundColor(back_color);
	panel->rect(x,y,total_width,1,false);
	panel->setBackgroundColor(bar_color);
	if (bar_width > 0) panel->rect(x,y,bar_width,1,false); // popøedí
	panel->setForegroundColor(TCODColor::white); // text
	panel->printCenter(x + total_width/2, y, TCOD_BKGND_NONE, "%s: %d/%d", name.c_str(), value, maximum);
}

// vykreslit všechno
void render_all() {
	TCODConsole::root->clear(); // všechno vyházet
	con->clear();

	for (Object** it = objects.begin(); it != objects.end(); it++) { // vykreslit entity
		(*it)->draw();
	}
	player->draw(); // hráèe vykreslit navrch

	for (int i = 0; i < MAP_WIDTH; i++) { // vykreslit bludištì
		for (int j = 0; j < MAP_HEIGHT; j++) {
			bool wall = map[i][j].blocks_sight;
			bool visible = fov_map.isInFov(i,j);
			if(visible) { // políèko je vidìt
				map[i][j].explored = true; // pak ho hráè prozkoumal
				if(wall) con->setBack(i,j,color_light_wall);
				else con->setBack(i,j,color_light_ground);				
			} else { // políèko není vidìt
				if (map[i][j].explored) { // vykreslíme jej jen pokud už ho hráè prozkoumal
					if(wall) con->setBack(i,j,color_dark_wall);
					else con->setBack(i,j,color_dark_ground);
				}
			}
		}
	}

	// vykreslení panelu
	panel->setBackgroundColor(TCODColor::black);
	panel->clear();

	// ukazatel HP
	render_bar(1, 1, BAR_WIDTH, "HP", player->fighter->hp, player->fighter->max_hp,
		TCODColor::lightRed, TCODColor::darkerRed);

	panel->setForegroundColor(TCODColor::white);
	int y = 1;
	for (Message* it = game_msgs.begin(); it < game_msgs.end(); it++) { // vypsat zprávy z msgboxu
		panel->setForegroundColor((*it).col);
		panel->printLeft(MSG_X,y,TCOD_BKGND_NONE,(*it).str.c_str());
		y += 1;
	}

	panel->setForegroundColor(TCODColor::grey);
	panel->printLeft(1,0,TCOD_BKGND_NONE,get_names_under_mouse().c_str()); // vypsat vysvìtlivky

	// vše vykreslit na obrazovku
	TCODConsole::blit(con, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TCODConsole::root, 0, 0);
	TCODConsole::blit(panel, 0, 0, SCREEN_WIDTH, PANEL_HEIGHT, TCODConsole::root, 0, PANEL_Y);
	TCODConsole::flush();
}

// reaguje na klávesy
void handle_keys() {
	TCOD_key_t key = TCODConsole::checkForKeypress(TCOD_KEY_PRESSED); // klávesa
	if (game_state == "playing") { // pokud hra probíhá
		if (key.vk == TCODK_UP) { // nahoru
			if (player->y > 0) player_move_or_attack(0,-1);
		} else if (key.vk == TCODK_DOWN) { // dolu
			if (player->y < SCREEN_HEIGHT - 1) player_move_or_attack(0,1);
		} else if (key.vk == TCODK_LEFT) { // doleva
			if (player->x > 0) player_move_or_attack(-1,0);
		} else if (key.vk == TCODK_RIGHT) { // doprava
			if (player->x < SCREEN_WIDTH - 1) player_move_or_attack(1,0);
		} else if (key.c == 'g') { // sebrat pøedmìt (pokud pod hráèem nìjaký je)
			for (Object **it = objects.begin(); it < objects.end(); it++) {
				if ((*it)->x == player->x && (*it)->y == player->y && (*it)->item != 0) {
					(*it)->item->pick_up();
					break;
				}
			}
		} else if (key.c == 'i') { // použít pøedmìt z inventáøe
			Item* chosen_item = inventory_menu("Press the key next to an item to use it, or any other to cancel.");
			if (chosen_item != 0) { // pokud hráè vybral pøedmìt, použít ho
				chosen_item->use();
			}
		} else if (key.c == 'd') { // vyhodit pøedmìt z inventáøe
			Item* chosen_item = inventory_menu("Press the key next to an item to drop it, or any other to cancel.");
			if (chosen_item != 0) { // pokud hráè vybral pøedmìt, položit ho
				chosen_item->drop();
			}
		}
	}
	if (key.vk == TCODK_ENTER && key.lalt) { // alt+enter = pøepnout fullscreen
		TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
	}
}

// uloží hru
void save_game() {
	TCODZip zip; // buffer
	for (int i = 0; i < MAP_WIDTH; i++) for (int j = 0; j < MAP_HEIGHT; j++) { // uložit bludištì
		zip.putInt(map[i][j].blocked);
		zip.putInt(map[i][j].blocks_sight);
		zip.putInt(map[i][j].explored);
	}
	zip.putInt(total_monsters); // uložit poèet pøíšerek
	zip.putInt(objects.size()); // poèet entit
	int player_index = 0;
	int i = 0;
	for (Object **it = objects.begin(); it < objects.end(); it++, i++) { // postupnì uloží všechny entity
		if (player==(*it)) player_index = i;
		zip.putInt((*it)->blocks); // data
		zip.putColor(&((*it)->color));
		zip.putChar((*it)->chr);
		zip.putString((*it)->name.c_str());
		zip.putInt((*it)->x);
		zip.putInt((*it)->y);
		if ((*it)->fighter != 0) { // má-li entita komponentu fighter
			zip.putInt(1); // pøíznak
			zip.putInt((*it)->fighter->hp); // data
			zip.putInt((*it)->fighter->max_hp);
			zip.putInt((*it)->fighter->pow);
			zip.putInt((*it)->fighter->def);
			if ((*it)->fighter->death == player_death) { // identifikátor funkce
				zip.putInt(1);
			} else zip.putInt(0);
		} else zip.putInt(0); // entita nemá komponentu fighter
		if ((*it)->ai != 0) { // má-li entita komponentu AI
			zip.putInt(1); // pøíznak
			zip.putString((*it)->ai->save().c_str()); // AI se ukládá samo
		} else zip.putInt(0); // entita nemá komponentu AI
		if ((*it)->item != 0) { // má-li entita komponentu Item
			zip.putInt(1); // pøíznak
		} else zip.putInt(0); // entita nemá komponentu Item
	}
	zip.putInt(player_index); // index hráèe v seznamu entit
	zip.putInt(inventory.size()); // velikost inventáøe
	for (Object **it = inventory.begin(); it < inventory.end(); it++) { // postupnì uloží celý inventáø
		zip.putInt((*it)->blocks); // data
		zip.putColor(&((*it)->color));
		zip.putChar((*it)->chr);
		zip.putString((*it)->name.c_str());
		zip.putInt((*it)->x);
		zip.putInt((*it)->y);
	}
	zip.putString(game_state.c_str()); // uloží stav hry
	zip.saveToFile("savegame"); // uloží buffer do souboru savegame
}

// naète hru
int load_game() {
	TCODZip zip; // buffer
	if (zip.loadFromFile("savegame") == 0) return -1; // není-li co naèíst, vrátí -1
	map = new Tile*[MAP_WIDTH]; // naètení bludištì
	for (int i = 0; i < MAP_WIDTH; i++) {
		map[i] = new Tile[MAP_HEIGHT];
		for (int j = 0; j < MAP_HEIGHT; j++) {
			map[i][j].blocked = zip.getInt();
			map[i][j].blocks_sight = zip.getInt();
			map[i][j].explored = zip.getInt();
		}
	}
	total_monsters = zip.getInt(); // naètení poètu pøíšerek
	int objects_size = zip.getInt(); // naètení poètu entit
	for (int i = 0; i < objects_size; i++) { // postupnì naplní entitami
		bool blocks = zip.getInt(); // data
		TCODColor color = zip.getColor();
		char chr = zip.getChar();
		string name = zip.getString();
		int x = zip.getInt();
		int y = zip.getInt();
		Object *o = new Object(x,y,chr,name,color,blocks); // vytvoøí entitu
		Fighter *f = 0; // komponenta Fighter
		if (zip.getInt() == 1) { // má-li
			int hp = zip.getInt(); // naètení dat
			int max_hp = zip.getInt();
			int pow = zip.getInt();
			int def = zip.getInt();
			if (zip.getInt() == 1) { // pøiøazení funkce
				f = new Fighter(max_hp,def,pow,player_death);
			} else {
				f = new Fighter(max_hp,def,pow,monster_death);
			}
			f->hp = hp;
			f->owner = o; // pøiøazení vlastníka
		}
		AI *a = 0; // komponenta AI
		if (zip.getInt() == 1) { // má-li
			string s = zip.getString(); // zpracování stringu
			a = new BasicMonster(); // zatím je vždy na konci BasicMonster
			a->owner = o; // pøiøazení vlastníka
			while (s.length() > 1) { // dokud nejsme na zaèátku
				size_t found = s.find_last_of("c"); // mezi posledním c a koncem je hodnota
				int n = atoi(s.substr(found+1).c_str()); // tu pøeèteme
				a = new ConfusedMonster(a,n); // pøidáme ConfusedMonster s patøiènými parametry
				a->owner = o; // pøiøazení vlastníka
				s = s.substr(0,found); // podíváme se na zbytek stringu
			}
		}
		Item *it = 0; // komponenta AI
		if (zip.getInt() == 1) { // má-li
			if (name == "healing potion") { // funkci pøiøadíme podle jména
				it = new Item(cast_heal);
			} else if (name == "scroll of lightning bolt") {
				it = new Item(cast_lightning);
			} else if (name == "scroll of confusion") {
				it = new Item(cast_confuse);
			} else if (name == "scroll of fireball") {
				it = new Item(cast_fireball);
			}
			it->owner = o; // pøiøazení vlastníka
		}
		o->fighter = f; // pøiøazení komponent
		o->ai = a;
		o->item = it;
		objects.push(o); // zaøazení do seznamu
	}
	player = objects.get(zip.getInt()); // index hráèe
	int inv_size = zip.getInt(); // naètení velikosti inventáøe
	for (int i = 0; i < inv_size; i++) { // postupnì naplní inventáø
		bool blocks = zip.getInt(); // data
		TCODColor color = zip.getColor();
		char chr = zip.getChar();
		string name = zip.getString();
		int x = zip.getInt();
		int y = zip.getInt();
		Item* it = 0; // komponenta Item
		if (name == "healing potion") { // pøiøazení funkce podle jména
			it = new Item(cast_heal);
		} else if (name == "scroll of lightning bolt") {
			it = new Item(cast_lightning);
		} else if (name == "scroll of confusion") {
			it = new Item(cast_confuse);
		} else if (name == "scroll of fireball") {
			it = new Item(cast_fireball);
		}
		inventory.push(new Object(x,y,chr,name,color,blocks,0,0,it)); // zaøazení do inventáøe
	}
	game_state = zip.getString(); // naètení stavu hry
	initialize_fov(); // inicializace FoV
	return 1;
}

// herní smyèka
void play_game() {
	bool was_said = false;
	while (!TCODConsole::isWindowClosed()) {

		if (!was_said) { // pokud to hra ještì neøekla
			was_said = true;
			if (game_state == "victory") {
				message("You are victorious!",TCODColor::red); // hráè vyhrál
			} else if (game_state == "dead") {
				message("You died!",TCODColor::red); // hráè prohrál
			} else was_said = false; // jinak není co øíct
		}

		// pokud jsou všechny pøíšerky mrtvé
		if (game_state == "playing" && total_monsters == 0) { // hráè vyhrál
			game_state = "victory";
		}

		player_action = "didnt-take-turn"; // hráè zatím netáhl

		handle_keys();

		if (fov_recompute) { // je tøeba pøepoèítat field of view?
			fov_recompute = false;
			fov_map.computeFov(player->x, player->y, TORCH_RADIUS, FOV_LIGHT_WALLS, FOV_ALGO);
		}

		if (game_state == "playing" && player_action != "didnt-take-turn") { // pokud hráè táhl, táhnou všechny pøíšerky
			for (Object **it = objects.begin(); it < objects.end(); it++) {
				if ((*it)->ai != 0) (*it)->ai->take_turn();
			}
		}

		render_all(); // všechno vykreslit
	}
	save_game(); // po ukonèení hry uložit
}

// inicializace nové hry
void new_game() {
	make_map(); // vytvoøí mapu
	initialize_fov(); // vytvoøí FoV

	// pøíjemná uvítací zpráva
	message("Welcome, stranger! Prepare to meet your doom!",TCODColor::red);
}

// hlavní menu
void main_menu() {
	TCODList<string> o; // možnosti hlavního menu
	o.push("Play a new game");
	o.push("Continue last game");
	o.push("Quit");
	TCODImage *pix = new TCODImage("dungeon.png");
	TCODConsole *window = new TCODConsole(24,5);
	window->setBackgroundColor(TCODColor::black);
	window->setForegroundColor(TCODColor::white);
	window->printCenter(11,1,TCOD_BKGND_NONE,"Yet Another Roguelike");
	window->printCenter(11,3,TCOD_BKGND_NONE,"by The Martin");
	while(!TCODConsole::isWindowClosed()) {
		TCODConsole::root->clear();
		pix->blit2x(TCODConsole::root,0,0);
		TCODConsole::blit(window,0,0,23,5,TCODConsole::root,SCREEN_WIDTH/2-12,10,1.0,0.7);
		int choice = menu("",o,24);
		if (choice == 0) { // nová hra
			new_game();
			play_game();
		} else if (choice == 1) { // naèíst
			if (load_game() == -1) {
				msg_box("No saved game to load.",24); // není co naèíst
			} else {
				play_game();
			}
		} else if (choice == 2) { // ukonèit
			break;
		}
	}
}

int main(int argc, char* argv[]) {
	// inicializace
	TCODConsole::initRoot(SCREEN_WIDTH,SCREEN_HEIGHT,"YARL",false);
	TCODSystem::setFps(FPS_LIMIT);
	TCODConsole::disableKeyboardRepeat();

	con->setBackgroundColor(TCODColor::black);

	main_menu();

	return 0;
}