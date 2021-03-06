#include "libtcod.hpp"
#include "SDL.h"
#include <string>
#include <sstream>
#include <cmath>

using namespace std;

// konstanty pro zobrazování
const int SCREEN_WIDTH = 80;
const int SCREEN_HEIGHT = 50;
const int FPS_LIMIT = 10;

// konstanty pro velikost a generování bludiště
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
const int DESCRIPTION_WIDTH = 50;

// konstanty pro zobrazování bludiště
TCODColor color_dark_ground(0,0,50);
TCODColor color_light_ground(0,0,100);
TCODColor color_dark_wall(50,50,100);
TCODColor color_light_wall(50,50,150);

const TCOD_fov_algorithm_t FOV_ALGO = FOV_BASIC;
const bool FOV_LIGHT_WALLS = true;
const int TORCH_RADIUS = 10;

// konstanty pro předměty
const int HEAL_AMOUNT = 4;
const int LIGHTNING_RANGE = 5;
const int LIGHTNING_DAMAGE = 20;
const int FIREBALL_RADIUS = 3;
const int FIREBALL_DAMAGE = 12;
const int CONFUSE_RANGE = 8;
const int CONFUSE_NUM_TURNS = 10;

// deklarace nejdůležitějších tříd
class Object;
class Fighter;
class AI;
class Item;
class Anim;

// string + barva
struct Message {
	string str;
	TCODColor col;
	bool operator==(const Message &m); // operátor== vyžadovaný třídou TCODList
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
	bool blocks; // znemožňuje průchod?
	Fighter *fighter; // komponenty
	AI *ai;
	Item *item;
	Object(int x_, int y_, char chr_, string name_, const TCODColor & color_, bool blocks_=false,
		Fighter *fighter_component=0, AI *ai_component=0, Item *item_component=0);
	void move(int dx, int dy); // popojít o (dx,dy)
	void move_towards(int tgt_x, int tgt_y); // popojít směrem k (tgt_x,tgt_y)
	void send_to_back(); // poslat na pozadí
	float distance_to(Object &other); // vzdálenost k jinému objektu
	float distance_to(int tgt_x, int tgt_y); // vzdálenost k (tgt_x,tgt_y)
	string get_description(); // vrátí popis
	void draw(); // vykreslení
};

// komponenta pro entity, které můžou bojovat
class Fighter {
public:
	Object* owner; // odkaz na vlastníka
	void (*death)(Object&); // funkce spuštěná při smrti
	int hp, max_hp, def, pow; // vlastnosti
	Fighter(int hp, int def, int pow, void (*death_function)(Object&) = 0):
		hp(hp), max_hp(hp), def(def), pow(pow), death(death_function) {;}
	void take_damage(int dmg); // obdržení damage
	void attack(Object &target); // útok na entitu target
	void heal(int amount); // ozdravení
};

// komponenta pro entity, které samy přemýšlí
// abstraktní třída
class AI {
public:
	Object* owner; // odkaz na vlastníka
	virtual void take_turn() = 0;
	virtual string save() = 0;
};

// základní příšerka, jen jde za hráčem
class BasicMonster: public AI {
public:
	virtual void take_turn();
	virtual string save();
};

// zmatená příšerka, chodí náhodně
class ConfusedMonster: public AI {
public:
	AI *old_ai; // AI, které příšerka měla původně
	int num_turns; // počet kol, po které příšerka ještě bude zmatená
	ConfusedMonster(AI *old_ai, int num_turns = CONFUSE_NUM_TURNS): old_ai(old_ai), num_turns(num_turns) {}
	virtual void take_turn();
	virtual string save();
};

// komponenta pro entity, které lze přidat do inventáře
class Item {
public:
	Object *owner; // odkaz na vlastníka
	int (*use_function)(); // samotná funkce předmětu
	Item(int (*use_function)() = 0): use_function(use_function) {}
	void pick_up(); // přidání předmětu z bludiště do inventáře
	void use(); // použití předmětu
	void drop(); // vrácení předmětu z inventáře do bludiště
};

// struktura pro animace
class Anim {
public:
	struct colchar {
		char c;
		TCODColor col;
	}; // znak + barva
	colchar*** _anim; // samotná animace
	int _n; // délka animace
	int _x,_y; // souřadnice levého horního rohu
	int _w,_h; // rozměry
	void init(); // inicializace _anim
	void play(); // přehrání animace
};

// animace pro svitek ohnivé koule
class AnimFireball: public Anim {
public:
	AnimFireball(int x, int y); // x,y - souřadnice cíle
};

// animace pro svitek blesku
class AnimLightning: public Anim {
public:
	AnimLightning(int x, int y, int tx, int ty); // x,y - souřadnice zdroje, tx,ty - souřadnice cíle
};

// animace pro svitek zmatení
class AnimConfuse: public Anim {
public:
	AnimConfuse(int x, int y); // x,y - souřadnice cíle
};

// animace pro léčení
class AnimHeal: public Anim {
public:
	AnimHeal(int x, int y); // x,y - souřadnice cíle
};

// políčko v bludišti
class Tile {
public:
	bool blocked, blocks_sight, explored; // brání v průchodu; není průhledné; hráč jej už viděl
	Tile(bool blocked = true): blocked(blocked), blocks_sight(blocked), explored(false) {;}
	Tile(bool blocked, bool blocks_sight): blocked(blocked), blocks_sight(blocks_sight), explored(false) {;}
};

// pomocná třída pro obdélník
class Rect {
public:
	int x1,x2,y1,y2,center_x,center_y;
	Rect(int x = 0, int y = 0, int w = 0, int h = 0):
		x1(x), y1(y), x2(x+w), y2(y+h), center_x(x + w/2), center_y(y + h/2) {;}
	bool intersect(Rect &other); // protíná obdélník nějaký jiný?
};

bool isBlocked(int x, int y); // je na políčku (x,y) něco, co brání v průchodu?
string numToStr(int); // pomocné funkce pro konverzi čísel na string
string numToStr(float);
string numToStr(double);
void target_tile(int &tgt_x, int &tgt_y, int max_range = -1); // GUI: vybrat políčko
Object* target_monster(int max_range = -1); // GUI: vybrat příšerku
void message(string new_msg,const TCODColor& color = TCODColor::white); // GUI: dát zprávu do msgboxu
string capitalize(string &s); // pomocná funkce pro velké první písmeno
float round(float _x); // pomocné funkce pro zaokrouhlování
double round(double _x);
void render_all(); // vykreslení všeho na obrazovku
string name_to_description(string &name);

Tile** map; // bludiště
Object* player; // hráč
TCODList<Object*> inventory; // inventář
TCODMap fov_map(MAP_WIDTH,MAP_HEIGHT); // dat. struktura pro field of view
TCODList<Object*> objects; // entity v bludišti
TCODList<Message> game_msgs; // GUI: msgbox
TCODConsole *con = new TCODConsole(SCREEN_WIDTH,SCREEN_HEIGHT); // bludiště
TCODConsole *panel = new TCODConsole(SCREEN_WIDTH,PANEL_HEIGHT); // informační panel
TCODRandom *rng = TCODRandom::getInstance(); // generátor náh. čísel
string game_state = "playing"; // stav hry
string player_action = " "; // hráčova akce
bool fov_recompute = true; // je potřeba přepočítat field of view?
int total_monsters; // celkový počet příšerek v bludišti

string player_desc = "It is you! What a handsome rogue indeed. Or roguette.";
string orc_desc = "An orc! What a foul, greenskinned beast. On its own barely a challenge.";
string troll_desc = "A troll! This large, lumbering hulk surely spells grave news for the unprepared!";
string heal_desc = "A healing potion! This tonic is sure to restore one's spirit and body right quick!";
string lightning_desc = "A scroll of lightning bolt! A powerful magick that will seek out the closest foe to strike.";
string fireball_desc = "A scroll of fireball! You can feel the embers of this magick craving to engulf the dungeon in fire.";
string confuse_desc = "A scroll of confusion! This magick is sure to scramble the mind of whomever you choose.";
string dead_desc = "A crumpled heap of meat and bones that used to be ";

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

// popojít k políčku (tgt_x,tgt_y)
void Object::move_towards(int tgt_x, int tgt_y) {
	int dx = tgt_x - x; // (dx,dy) směrový vektor k cíli
	int dy = tgt_y - y;
	float distance = sqrtf(dx*dx + dy*dy);
	dx = int(round(float(dx)/distance)); // normalizace vektoru
	dy = int(round(float(dy)/distance));
	move(dx,dy);
}

// spočte vzdálenost k entitě
float Object::distance_to(Object &other) {
	int dx = other.x - x;
	int dy = other.y - y;
	return sqrtf(dx*dx + dy*dy);
}

// spočte vzdálenost k políčku
float Object::distance_to(int tgt_x, int tgt_y) {
	int dx = tgt_x - x;
	int dy = tgt_y - y;
	return sqrtf(dx*dx + dy*dy);
}

// pošle entitu na začátek vykreslovacího seznamu
// (bude vykreslená první -> bude na pozadí)
void Object::send_to_back() {
	objects.remove(this);
	objects.insertBefore(this,0);
}

// vrátí popis entity
string Object::get_description() {
	string s = name_to_description(name);
	if (fighter) {
		s += "\n\n";
		float injured = (fighter->hp)/(fighter->max_hp);
		if (injured > 0.8) s += "Appears mostly healthy.";
		else if (injured > 0.5 && injured <= 0.8) s += "Appears wounded.";
		else if (injured > 0.2 && injured <= 0.5) s += "Appears grievously wounded.";
		else if (injured <= 0.2) s += "Appears as if on the brink of death.";
	}
	return s;
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

// zranění bojovníka
void Fighter::take_damage(int dmg) {
	if (dmg > 0) hp -= dmg;
	if (hp <= 0 && death != 0) death(*owner); // je-li hp < 0, spustit funkci pro úmrtí
}

// zaútočit na entitu
void Fighter::attack(Object &target) {
	int damage = pow - target.fighter->def; // spočíst dmg
	if (damage > 0) { // je-li vůbec nějaké
		message(capitalize(owner->name) + " attacks " + target.name + " for " + numToStr(damage) + " hit points.");
		target.fighter->take_damage(damage); // zranit entitu
	} else { // jinak
		message(capitalize(owner->name) + " attacks " + target.name + " but it has no effect.");
	}
}

// vyléčit
void Fighter::heal(int amount) {
	hp += amount;
	if (hp > max_hp) hp = max_hp; // nelze vyléčit nad max_hp
}

// základní AI příšerky
void BasicMonster::take_turn() {
	if (fov_map.isInFov(owner->x, owner->y)) { // je-li příšerka viditelná hráčem (<==> příšerka vidí hráče)
		if (owner->distance_to(*player) >= 2.0) { // je-li dál než 2
			owner->move_towards(player->x, player->y); // jít k hráči
		} else if(player->fighter->hp > 0) {
			owner->fighter->attack(*player); // jinak zaútočit na hráče
		}
	}
}

// výpis
string BasicMonster::save() {
	return "b";
}

// AI zmatené příšerky
void ConfusedMonster::take_turn() {
	if (num_turns > 0) { // zbývá-li nějaké zmatení
		owner->move(rng->getInt(-1,1),rng->getInt(-1,1)); // náhodně popojít
		num_turns -= 1; // posunout počítadlo
	} else { // jinak
		owner->ai = old_ai; // vrátit původní AI
		message("The " + owner->name + " is no longer confused!", TCODColor::red);
		delete this; // tohle už nepotřebujeme
	}
}

// výpis
string ConfusedMonster::save() {
	return "c" + numToStr(num_turns) + old_ai->save(); // zapíše "c", počet zbývajících kol a vypíše old_ai
}

// sebrat předmět
void Item::pick_up() {
	if (inventory.size() >= 26) { // nelze mít více než 26 předmětů v inventáři
		message("Your inventory is full, cannot pick up " + owner->name + ".", TCODColor::red);
	} else {
		inventory.push(owner); // přidat předmět do inventáře
		objects.remove(owner); // odebrat předmět z bludiště
		message("You picked up a " + owner->name + "!", TCODColor::green);
	}
}

// použít předmět
void Item::use() {
	if (use_function == 0) { // pokud předmět nelze použít
		message("The " + owner->name + " cannot be used.");
	} else { // jinak
		if (use_function() != -1) { // pokud použití dopadlo úspěšně
			inventory.remove(owner); // odebrat předmět z inventáře
		}
	}
}

// zahodit předmět
void Item::drop() {
	objects.push(owner); // přidat předmět do bludiště
	inventory.remove(owner); // odebrat předmět z inventáře
	owner->x = player->x; // přesunout předmět pod hráče
	owner->y = player->y;
	message("You dropped a " + owner->name + ".",TCODColor::yellow);
}

// přehrát animaci
void Anim::play() {
	render_all(); // smazat všechna menu atp.
	TCODConsole *win = new TCODConsole(_w,_h); // konzole pro animaci
	TCODConsole *back = new TCODConsole(*TCODConsole::root); // uložený snímek obrazovky
	for (int k = 0; k < _n; k++) { // pro všechny kroky animace
		win->clear(); // vymazat konzoli
		for (int i = 0; i < _w; i++) for (int j = 0; j < _h; j++) {
			win->setFore(i,j,_anim[i][j][k].col); // vyplnit konzoli podle struktury animace
			win->setChar(i,j,_anim[i][j][k].c);
		}
		TCODConsole::blit(back,0,0,back->getWidth(),back->getHeight(),TCODConsole::root,0,0); // přeblitovat
		TCODConsole::blit(win,0,0,_w,_h,TCODConsole::root,_x,_y,1.0,0.0);
		TCODConsole::root->flush(); // vykreslit
	}
}

// inicializovat strukturu
void Anim::init() {
	_anim = new colchar**[_w];
	for (int i = 0; i < _w; i++) {
		_anim[i] = new colchar*[_h];
		for (int j = 0; j < _h; j++) {
			_anim[i][j] = new colchar[_n];
			for (int k = 0; k < _n; k++) {
				_anim[i][j][k].c = 32;
				_anim[i][j][k].col = TCODColor::white;
			}
		}
	}
}

// animace ohnivé koule
AnimFireball::AnimFireball(int x, int y) {
	_n = FIREBALL_RADIUS + 3;
	_x = x - FIREBALL_RADIUS;
	_y = y - FIREBALL_RADIUS;
	_w = 2*FIREBALL_RADIUS + 1;
	_h = 2*FIREBALL_RADIUS + 1;
	int c = FIREBALL_RADIUS;
	init();
	for (int i = 0; i < _w; i++) for (int j = 0; j < _h; j++) { // pro všechny buňky
		float dx = i - c;
		float dy = j - c;
		float dist = sqrtf(dx*dx + dy*dy);
		if (dist <= FIREBALL_RADIUS) { // je-li buňka zasažena
			if (fov_map.isInFov(_x+i,_y+j)) { // a je-li vidět hráčem
				int idist = round(dist); // vzdálenost udává zpoždění
				_anim[i][j][idist].c = '#'; // napřed #
				_anim[i][j][idist+1].c = 'o'; // pak o
				_anim[i][j][idist+2].c = '.'; // pak .
				_anim[i][j][idist].col = TCODColor::orange;
				_anim[i][j][idist+1].col = TCODColor::orange;
				_anim[i][j][idist+2].col = TCODColor::orange;
			}
		}
	}
}

// animace blesku
AnimLightning::AnimLightning(int x, int y, int tx, int ty) {
	_n = 3;
	_x = min(x,tx);
	_y = min(y,ty);
	_w = abs(x-tx)+1;
	_h = abs(y-ty)+1;
	init();
	int i = (tx > x) ? 0 : _w-1; // zdroj blesku
	int j = (ty > y) ? 0 : _h-1;
	int ti = (i == 0) ? _w-1 : 0; // cíl
	int tj = (j == 0) ? _h-1 : 0;
	TCODLine::init(i,j,ti,tj); // odkrokovat
	do {
		_anim[i][j][0].c = '+'; // v každém políčku problesknout +
		_anim[i][j][0].col = TCODColor::cyan;
		_anim[i][j][2].c = '+';
		_anim[i][j][2].col = TCODColor::cyan;
	} while(!TCODLine::step(&i,&j));
}

// animace zmatení
AnimConfuse::AnimConfuse(int x, int y) {
	_n = 5;
	_x = x-1;
	_y = y-1;
	_w = 3;
	_h = 3;
	init();
	for (int i = 0; i < 3; i++) { // náhodně umístit po sobě tři tmavnoucí otazníky kolem cíle
		int d = rng->getInt(0,7);
		int ix = 1, iy = 1;
		if (d == 0 || d == 1 || d == 2) ix = 0;
		else if (d == 4 || d == 5 || d == 6) ix = 2;
		if (d == 2 || d == 3 || d == 4) iy = 2;
		else if (d == 6 || d == 7 || d == 8) iy = 0;
		_anim[ix][iy][i].c = '?';
		_anim[ix][iy][i+1].c = '?';
		_anim[ix][iy][i+2].c = '?';
		_anim[ix][iy][i].col = TCODColor::white;
		_anim[ix][iy][i+1].col = TCODColor::lightGrey;
		_anim[ix][iy][i+2].col = TCODColor::darkGrey;
	}
}

// animace léčení
AnimHeal::AnimHeal(int x, int y) {
	_n = 5;
	_x = x-1;
	_y = y-1;
	_w = 3;
	_h = 3;
	init();
	for (int i = 0; i < 3; i++) { // náhodně umístit po sobě tři tmavnoucí + kolem cíle
		int d = rng->getInt(0,7);
		int ix = 1, iy = 1;
		if (d == 0 || d == 1 || d == 2) ix = 0;
		else if (d == 4 || d == 5 || d == 6) ix = 2;
		if (d == 2 || d == 3 || d == 4) iy = 2;
		else if (d == 6 || d == 7 || d == 8) iy = 0;
		_anim[ix][iy][i].c = '+';
		_anim[ix][iy][i+1].c = '+';
		_anim[ix][iy][i+2].c = '+';
		_anim[ix][iy][i].col = TCODColor::white;
		_anim[ix][iy][i+1].col = TCODColor::lightGreen;
		_anim[ix][iy][i+2].col = TCODColor::darkGreen;
	}
}

// přehraje animaci
void playAnim(Anim *a) {
	a->play();
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
	if (r[0] >= 'a' && r[0] <= 'z') r[0] -= 32;
	return r;
}

float round(float _x) {
	return floor(_x + 0.5);
}

double round(double _x) {
	return floor(_x + 0.5);
}

string name_to_description(string &name) {
	if (name == "player") return player_desc;
	if (name == "orc") return orc_desc;
	if (name == "troll") return troll_desc;
	if (name == "healing potion") return heal_desc;
	if (name == "scroll of lightning bolt") return lightning_desc;
	if (name == "scroll of confusion") return confuse_desc;
	if (name == "scroll of fireball") return fireball_desc;
	if (name.substr(0,10) == "remains of") return dead_desc + name.substr(10) + ".";
	return "Don't know what that is, sorry.";
}

// brání na políčku (x,y) něco v průchodu?
bool isBlocked(int x, int y) {
	if (map[x][y].blocked) return true; // v průchodu brání bludiště
	for (Object** it = objects.begin(); it != objects.end(); it++) {
		if ((*it)->blocks && (*it)->x == x && (*it)->y == y) return true; // v průchodu brání entita
	}
	return false; // v průchodu nebrání nic
}

// vrátí nejbližší viditelnou příšerku bližší než je range
Object* closest_monster(int range) {
	Object* closest_enemy = 0; // nejbližší zatím nalezená příšerka
	int closest_dist = range + 1; // její vzdálenost k hráči

	for (Object **it = objects.begin(); it != objects.end(); it++) { // pro všechny viditelné příšerky
		if((*it)->fighter != 0 && (*it) != player && fov_map.isInFov((*it)->x,(*it)->y)) {
			float dist = player->distance_to(**it);
			if (dist < closest_dist) { // je-li příšerka bližší než zatím nejbližší nalezená
				closest_enemy = (*it); // je příšerka nová nejbližší nalezená
				closest_dist = dist;
			}
		}
	}
	return closest_enemy;
}

// zalamování řádků
TCODList<Message> text_wrap(Message &m, int width) {
	TCODList<Message> ret;
	size_t found = 0;
	while (1) {
		if (m.str.length() < width) { // zbytek se vejde do řádku
			ret.push(m);
			break;
		}
		found = m.str.find_last_of(" ",width); // najít místo zalomení
		if (found == string::npos) found = width;
		Message new_m = {m.str.substr(0,found),m.col}; // zalomit
		ret.push(new_m);
		m.str = m.str.substr(found+1); // se zbytkem udělat totéž
	}
	return ret;
}

// GUI: vybrat viditelné políčko bližší než max_range (souřadnice se zapíší do tgt_x a tgt_y)
// max_range == -1 -> max_range je nekonečno
void target_tile(int &tgt_x, int &tgt_y, int max_range) {
	while(1) {
		render_all(); // vykreslíme všechno (tj. odstraníme inventář)
		TCODConsole::flush();

		TCOD_key_t key = TCODConsole::checkForKeypress();
		TCOD_mouse_t mouse = TCODMouse::getStatus();
		if (key.vk == TCODK_ENTER && key.lalt) { // alt+enter = přepnout fullscreen
			TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
		}
		if (mouse.lbutton_pressed && fov_map.isInFov(mouse.cx,mouse.cy) &&
			(max_range == -1 || player->distance_to(mouse.cx,mouse.cy) < max_range)) {
			// hráč kliknul na viditelné políčko, které je blíže než max_range
			tgt_x = mouse.cx; // vrátit souřadnice políčka
			tgt_y = mouse.cy;
			return;
		}
		if (mouse.rbutton_pressed || key.vk == TCODK_ESCAPE) { // zrušit výběr
			tgt_x = -1; // vrátit neplatnou hodnotu
			tgt_y = -1;
			return;
		}
	}
}

// GUI: vybrat viditelnou příšerku bližší než max_range
// max_range == -1 -> max_range je nekonečno
Object* target_monster(int max_range) {
	while(1) {
		int x,y;
		target_tile(x,y,max_range);
		if (x == -1) { // hráč zrušil výběr
			return 0;
		}
		for (Object **it = objects.begin(); it != objects.end(); it++) {
			if((*it)->x == x && (*it)->y == y && (*it)->fighter != 0 && (*it) != player) {
				// pokud hráč kliknul na příšerku, vrátíme ji
				return (*it);
			}
		}
	}
}

// GUI: vypíše zprávu do msgboxu
void message(string new_msg, const TCODColor &color) {
	Message m = {new_msg,color};
	game_msgs.addAll(text_wrap(m,MSG_WIDTH)); // přidat zalomenou zprávu do seznamu
	while(game_msgs.size() > MSG_HEIGHT) { // pokud je seznam moc dlouhý, zkrátit
		game_msgs.remove(game_msgs.get(0));
	}
}

// GUI: zobrazí menu s hlavičkou header, možnostmi options a šířkou width
int menu(string header, TCODList<string> &options, int width) {
	int header_height = con->getHeightLeftRect(0,0,width,0,header.c_str()); // počet řádků hlavičky
	if (header == "") header_height = 0;
	int height = options.size() + header_height + 2; // výška menu
	if (options.isEmpty()) height -= 1;
	TCODConsole *window = new TCODConsole(width,height); // pomocná konzole pro menu
	window->setForegroundColor(TCODColor::white);
	window->printLeftRect(0,0,width,0,TCOD_BKGND_NONE,header.c_str());
	int y = header_height + 1;
	char letter_index = 'a'; // začneme u 'a'
	for (string* it = options.begin(); it != options.end(); it++) { // vypíše všechny možnosti
		string letter(1,letter_index);
		string text = "(" + letter + ") " + (*it); // včetně písmenka, které je potřeba stisknout
		window->printLeft(0,y,TCOD_BKGND_NONE,text.c_str());
		y += 1;
		letter_index += 1;
	}
	int x = SCREEN_WIDTH/2 - width/2;
	y = SCREEN_HEIGHT/2 - height/2;
	TCODConsole::blit(window, 0, 0, width, height, TCODConsole::root, x, y, 1.0, 0.7); // vykreslí menu
	TCODConsole::root->flush();
	TCOD_key_t key = TCODConsole::waitForKeypress(true); // čeká na volbu
	if (key.vk == TCODK_ENTER && key.lalt) { // alt+enter = přepnout fullscreen
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

// vypíše inventář a vrátí hráčem vybraný předmět
Item* inventory_menu(string header) {
	TCODList<string> options;
	if (inventory.size() == 0) { // je-li inventář prázdný
		options.push("Inventory is empty."); // upozorní hráče
	} else { // jinak
		for (Object **it = inventory.begin(); it != inventory.end(); it++) {
			options.push((*it)->name); // vypíše inventář
		}
	}
	int index = menu(header, options, INVENTORY_WIDTH); // nechá hráče vybrat
	if (index == -1 || inventory.size() == 0) return 0; // je-li volba neplatná, nevrátí žádný předmět
	return inventory.get(index)->item; // jinak vrátí vybraný předmět
}

// funkce pro uzdravovací lektvar
int cast_heal() {
	if (player->fighter->hp == player->fighter->max_hp) { // je-li hráč zdravý
		message("You are already at full health.", TCODColor::red);
		return -1; // lektvar nebyl použit
	}
	message("Your wounds start to feel better.", TCODColor::violet);
	player->fighter->heal(HEAL_AMOUNT); // uzdraví hráče
	playAnim(new AnimHeal(player->x,player->y));
	return 1; // vrátí úspěch
}

// funkce pro svitek blesku
int cast_lightning() {
	Object* monster = closest_monster(LIGHTNING_RANGE); // zamíří na nejbližší příšerku v dosahu
	if (monster == 0) { // pokud není žádná v dosahu
		message("No monster is close enough to strike.", TCODColor::red);
		return -1; // svitek nebyl použit
	}
	message("A lightning bolt strikes the " + monster->name + " for " + numToStr(LIGHTNING_DAMAGE) + " hit points.",TCODColor::lightBlue);
	monster->fighter->take_damage(LIGHTNING_DAMAGE); // poraní příšerku
	playAnim(new AnimLightning(player->x,player->y,monster->x,monster->y));
	return 1; // vrátí úspěch
}

// funkce pro svitek ohnivé koule
int cast_fireball() {
	message("Left-click a target tile for the fireball, or right-click to cancel.", TCODColor::lightCyan);
	int x, y;
	target_tile(x,y); // nechá hráče vybrat cíl
	if (x == -1) { // pokud nebyl vybrán platný cíl
		message("Spell cancelled.", TCODColor::cyan);
		return -1; // svitek nebyl použit
	}
	message("The fireball explodes, burning everything within " + numToStr(FIREBALL_RADIUS) + " tiles!", TCODColor::orange);

	for (Object **it = objects.begin(); it != objects.end(); it++) { // poraní všechny příšerky (a hráče) v dosahu
		if ((*it)->distance_to(x,y) <= FIREBALL_RADIUS && (*it)->fighter != 0) {
			message("The " + (*it)->name + " gets burned for " + numToStr(FIREBALL_DAMAGE) + " hit points.", TCODColor::orange);
			(*it)->fighter->take_damage(FIREBALL_DAMAGE);
		}
	}
	playAnim(new AnimFireball(x,y));
	return 1;
}

// funkce pro svitek omámení
int cast_confuse() {
	message("Left-click an enemy to confuse it, or right-click to cancel.", TCODColor::lightCyan);
	Object *monster = target_monster(CONFUSE_RANGE); // nechá hráče vybrat cíl
	if (monster == 0) { // pokud nebyl vybrán cíl
		message("Spell cancelled.", TCODColor::cyan);
		return -1; // svitek nebyl použit
	}

	AI* old_ai = monster->ai;
	monster->ai = new ConfusedMonster(old_ai); // AI příšerky se nahradí zmateným AI
	monster->ai->owner = monster;
	message("The eyes of the " + monster->name + " glaze over, as it starts to stumble around!", TCODColor::orange);
	playAnim(new AnimConfuse(monster->x,monster->y));
	return 1;
}

// podívat se na entity na x,y
void look_at(int x, int y) {
	TCODList<Object*> objs;
	for (Object **it = objects.begin(); it != objects.end(); it++) { // seznam všech entit v (x,y)
		if ((*it)->x == x && (*it)->y == y) objs.push(*it);
	}
	if (objs.isEmpty()) msg_box("Naught of note.",DESCRIPTION_WIDTH); // nic tam není
	else {
		int oindex = 0;
		if (objs.size() > 1) { // je tam víc než jedna věc - na co se chce hráč podívat?
			TCODList<string> names;
			for (Object **it = objs.begin(); it != objs.end(); it++) {
				names.push((*it)->name);
			}
			oindex = menu("Press the key next to an entity to look at it, or any other to cancel.",names,INVENTORY_WIDTH);
			render_all(); // smazat menu
		}
		if (oindex > -1) msg_box(objs.get(oindex)->get_description(),DESCRIPTION_WIDTH);
	}
}

// vrátí jména entit pod kurzorem, oddělená čárkami a s prvním písmenem velkým
string get_names_under_mouse() {
	TCOD_mouse_t mouse = TCODMouse::getStatus();
	string ret;
	for (Object **it = objects.begin(); it != objects.end(); it++) {
		// přidá do stringu ret jména všech entit pod kurzorem
		if ((*it)->x == mouse.cx && (*it)->y == mouse.cy && fov_map.isInFov((*it)->x,(*it)->y)) {
			ret += (*it)->name + ", ";
		}
	}
	if (ret.length() > 0) ret.erase(ret.end()-2,ret.end()); // umažeme poslední čárku
	return capitalize(ret);
}

// funkce pro smrt hráče
void player_death(Object& plr) {
	game_state = "dead"; // nastaví stav hry
	plr.chr = '%'; // promění hráče v mrtvolu
	plr.color = TCODColor::darkRed;
}

// funkce pro smrt příšerky
void monster_death(Object& monster) {
	monster.chr = '%'; // promění příšerku v mrtvolu
	monster.color = TCODColor::darkRed;
	monster.blocks = false; // přes mrtvolu lze procházet
	delete monster.fighter; // s mrtvolou nelze bojovat
	monster.fighter = 0;
	delete monster.ai; // mrtvola nemyslí
	monster.ai = 0;
	message(capitalize(monster.name) + " is dead!",TCODColor::orange);
	monster.name = "remains of " + monster.name; // změní jméno
	monster.send_to_back(); // vrátí na konec
	total_monsters -= 1;
}

// funkce pro pohyb/útok hráče
void player_move_or_attack(int dx, int dy) {
	int x = player->x + dx;
	int y = player->y + dy;

	Object* target = 0;
	for (Object ** it = objects.begin(); it != objects.end(); it++) {
		// vrazil hráč do příšerky?
		if ((*it)->fighter != 0 && (*it)->x == x && (*it)->y == y) {
			target = (*it);
			break;
		}
	}

	if (target != 0) { // pokud ano, tak na ni zaútočil
		player_action = "attack";
		player->fighter->attack(*target);
	} else { // jinak se jen pohnul
		player->move(dx,dy);
		player_action = "move";
		fov_recompute = true; // pak je potřeba přepočítat FoV
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
	// vygeneruje příšerky
	int num_monsters = rng->getInt(0,MAX_ROOM_MONSTERS);
	for (int i = 0; i < num_monsters; i++) {
		int x = rng->getInt(room.x1 + 1,room.x2 - 1); // umístění
		int y = rng->getInt(room.y1 + 1,room.y2 - 1);
		int choice = rng->getInt(0,100); // výběr typu
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
	// vygeneruje předměty
	int num_items = rng->getInt(0,MAX_ROOM_ITEMS);
	for (int i = 0; i < num_items; i++) {
		int x = rng->getInt(room.x1 + 1,room.x2 - 1);
		int y = rng->getInt(room.y1 + 1,room.y2 - 1);
		if (!isBlocked(x,y)) { // je-li místo volné
			int choice = rng->getInt(0,100); // výběr typu
			Object *item;
			if (choice < 70) {
				item = new Object(x,y,'!',"healing potion",TCODColor::violet,false,
					0,0,new Item(cast_heal));
			} else if (choice < 80) {
				item = new Object(x,y,'#',"scroll of lightning bolt",TCODColor::lightYellow,false,
					0,0,new Item(cast_lightning));
			} else if (choice < 90) {
				item = new Object(x,y,'#',"scroll of confusion",TCODColor::lightYellow,false,
					0,0,new Item(cast_confuse));
			} else {
				item = new Object(x,y,'#',"scroll of fireball",TCODColor::lightYellow,false,
					0,0,new Item(cast_fireball));
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

// vygeneruje bludiště
void make_map() {
	player = new Object(0, 0, '@', "player", TCODColor::white, true, new Fighter(30,2,5,player_death));
	objects.push(player);

	map = new Tile*[MAP_WIDTH]; // napřed je vše prázdné
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
		bool failed = false; // protíná se nová místnost s už nějakou vygenerovanou?
		for (Rect* it = rooms.begin(); it != rooms.end(); it++) {
			if (new_room.intersect(*it)) {
				failed = true;
				break;
			}
		}
		if (!failed) { // pokud ne, přidá se do mapy
			create_room(new_room);
			int new_x = new_room.center_x;
			int new_y = new_room.center_y;
			if (num_rooms == 0) { // je-li to první místnost, bude v ní hráč
				player->x = new_x;
				player->y = new_y;
			} else { // jinak se spojí s předchozí vygenerovanou místností tunely
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

// GUI: vykreslí ukazatel hodnoty na souřadnicích (x,y) o daných barvách
void render_bar(int x, int y, int total_width, string name, int value, int maximum,
				const TCODColor &bar_color, const TCODColor &back_color) {
	int bar_width = int(float(total_width*value)/maximum); // pozadí
	panel->setBackgroundColor(back_color);
	panel->rect(x,y,total_width,1,false);
	panel->setBackgroundColor(bar_color);
	if (bar_width > 0) panel->rect(x,y,bar_width,1,false); // popředí
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
	player->draw(); // hráče vykreslit navrch

	for (int i = 0; i < MAP_WIDTH; i++) { // vykreslit bludiště
		for (int j = 0; j < MAP_HEIGHT; j++) {
			bool wall = map[i][j].blocks_sight;
			bool visible = fov_map.isInFov(i,j);
			if(visible) { // políčko je vidět
				map[i][j].explored = true; // pak ho hráč prozkoumal
				if(wall) con->setBack(i,j,color_light_wall);
				else con->setBack(i,j,color_light_ground);				
			} else { // políčko není vidět
				if (map[i][j].explored) { // vykreslíme jej jen pokud už ho hráč prozkoumal
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
	for (Message* it = game_msgs.begin(); it != game_msgs.end(); it++) { // vypsat zprávy z msgboxu
		panel->setForegroundColor((*it).col);
		panel->printLeft(MSG_X,y,TCOD_BKGND_NONE,(*it).str.c_str());
		y += 1;
	}

	panel->setForegroundColor(TCODColor::grey);
	panel->printLeft(1,0,TCOD_BKGND_NONE,get_names_under_mouse().c_str()); // vypsat vysvětlivky

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
		} else if (key.c == 'g') { // sebrat předmět (pokud pod hráčem nějaký je)
			for (Object **it = objects.begin(); it != objects.end(); it++) {
				if ((*it)->x == player->x && (*it)->y == player->y && (*it)->item != 0) {
					(*it)->item->pick_up();
					break;
				}
			}
		} else if (key.c == 'i') { // použít předmět z inventáře
			Item* chosen_item = inventory_menu("Press the key next to an item to use it, or any other to cancel.");
			if (chosen_item != 0) { // pokud hráč vybral předmět, použít ho
				chosen_item->use();
			}
		} else if (key.c == 'd') { // vyhodit předmět z inventáře
			Item* chosen_item = inventory_menu("Press the key next to an item to drop it, or any other to cancel.");
			if (chosen_item != 0) { // pokud hráč vybral předmět, položit ho
				chosen_item->drop();
			}
		} else if (key.c == 'l') { // podívat se na entitu
			message("Left-click a target tile to look at, or right-click to cancel.", TCODColor::lightCyan);
			int tx,ty;
			target_tile(tx,ty);
			if (tx == -1) {
				message("Cancelled.", TCODColor::lightCyan);
			} else look_at(tx,ty);
		}
	}
	if (key.vk == TCODK_ENTER && key.lalt) { // alt+enter = přepnout fullscreen
		TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
	}
}

// uloží hru
void save_game() {
	TCODZip zip; // buffer
	for (int i = 0; i < MAP_WIDTH; i++) for (int j = 0; j < MAP_HEIGHT; j++) { // uložit bludiště
		zip.putInt(map[i][j].blocked);
		zip.putInt(map[i][j].blocks_sight);
		zip.putInt(map[i][j].explored);
	}
	zip.putInt(total_monsters); // uložit počet příšerek
	zip.putInt(objects.size()); // počet entit
	int player_index = 0;
	int i = 0;
	for (Object **it = objects.begin(); it != objects.end(); it++, i++) { // postupně uloží všechny entity
		if (player==(*it)) player_index = i;
		zip.putInt((*it)->blocks); // data
		zip.putColor(&((*it)->color));
		zip.putChar((*it)->chr);
		zip.putString((*it)->name.c_str());
		zip.putInt((*it)->x);
		zip.putInt((*it)->y);
		if ((*it)->fighter != 0) { // má-li entita komponentu fighter
			zip.putInt(1); // příznak
			zip.putInt((*it)->fighter->hp); // data
			zip.putInt((*it)->fighter->max_hp);
			zip.putInt((*it)->fighter->pow);
			zip.putInt((*it)->fighter->def);
			if ((*it)->fighter->death == player_death) { // identifikátor funkce
				zip.putInt(1);
			} else zip.putInt(0);
		} else zip.putInt(0); // entita nemá komponentu fighter
		if ((*it)->ai != 0) { // má-li entita komponentu AI
			zip.putInt(1); // příznak
			zip.putString((*it)->ai->save().c_str()); // AI se ukládá samo
		} else zip.putInt(0); // entita nemá komponentu AI
		if ((*it)->item != 0) { // má-li entita komponentu Item
			zip.putInt(1); // příznak
		} else zip.putInt(0); // entita nemá komponentu Item
	}
	zip.putInt(player_index); // index hráče v seznamu entit
	zip.putInt(inventory.size()); // velikost inventáře
	for (Object **it = inventory.begin(); it != inventory.end(); it++) { // postupně uloží celý inventář
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

// načte hru
int load_game() {
	TCODZip zip; // buffer
	if (zip.loadFromFile("savegame") == 0) return -1; // není-li co načíst, vrátí -1
	map = new Tile*[MAP_WIDTH]; // načtení bludiště
	for (int i = 0; i < MAP_WIDTH; i++) {
		map[i] = new Tile[MAP_HEIGHT];
		for (int j = 0; j < MAP_HEIGHT; j++) {
			map[i][j].blocked = zip.getInt();
			map[i][j].blocks_sight = zip.getInt();
			map[i][j].explored = zip.getInt();
		}
	}
	total_monsters = zip.getInt(); // načtení počtu příšerek
	int objects_size = zip.getInt(); // načtení počtu entit
	for (int i = 0; i < objects_size; i++) { // postupně naplní entitami
		bool blocks = zip.getInt(); // data
		TCODColor color = zip.getColor();
		char chr = zip.getChar();
		string name = zip.getString();
		int x = zip.getInt();
		int y = zip.getInt();
		Object *o = new Object(x,y,chr,name,color,blocks); // vytvoří entitu
		Fighter *f = 0; // komponenta Fighter
		if (zip.getInt() == 1) { // má-li
			int hp = zip.getInt(); // načtení dat
			int max_hp = zip.getInt();
			int pow = zip.getInt();
			int def = zip.getInt();
			if (zip.getInt() == 1) { // přiřazení funkce
				f = new Fighter(max_hp,def,pow,player_death);
			} else {
				f = new Fighter(max_hp,def,pow,monster_death);
			}
			f->hp = hp;
			f->owner = o; // přiřazení vlastníka
		}
		AI *a = 0; // komponenta AI
		if (zip.getInt() == 1) { // má-li
			string s = zip.getString(); // zpracování stringu
			a = new BasicMonster(); // zatím je vždy na konci BasicMonster
			a->owner = o; // přiřazení vlastníka
			while (s.length() > 1) { // dokud nejsme na začátku
				size_t found = s.find_last_of("c"); // mezi posledním c a koncem je hodnota
				int n = atoi(s.substr(found+1).c_str()); // tu přečteme
				a = new ConfusedMonster(a,n); // přidáme ConfusedMonster s patřičnými parametry
				a->owner = o; // přiřazení vlastníka
				s = s.substr(0,found); // podíváme se na zbytek stringu
			}
		}
		Item *it = 0; // komponenta AI
		if (zip.getInt() == 1) { // má-li
			if (name == "healing potion") { // funkci přiřadíme podle jména
				it = new Item(cast_heal);
			} else if (name == "scroll of lightning bolt") {
				it = new Item(cast_lightning);
			} else if (name == "scroll of confusion") {
				it = new Item(cast_confuse);
			} else if (name == "scroll of fireball") {
				it = new Item(cast_fireball);
			}
			it->owner = o; // přiřazení vlastníka
		}
		o->fighter = f; // přiřazení komponent
		o->ai = a;
		o->item = it;
		objects.push(o); // zařazení do seznamu
	}
	player = objects.get(zip.getInt()); // index hráče
	int inv_size = zip.getInt(); // načtení velikosti inventáře
	for (int i = 0; i < inv_size; i++) { // postupně naplní inventář
		bool blocks = zip.getInt(); // data
		TCODColor color = zip.getColor();
		char chr = zip.getChar();
		string name = zip.getString();
		int x = zip.getInt();
		int y = zip.getInt();
		Item* it = 0; // komponenta Item
		if (name == "healing potion") { // přiřazení funkce podle jména
			it = new Item(cast_heal);
		} else if (name == "scroll of lightning bolt") {
			it = new Item(cast_lightning);
		} else if (name == "scroll of confusion") {
			it = new Item(cast_confuse);
		} else if (name == "scroll of fireball") {
			it = new Item(cast_fireball);
		}
		inventory.push(new Object(x,y,chr,name,color,blocks,0,0,it)); // zařazení do inventáře
	}
	game_state = zip.getString(); // načtení stavu hry
	initialize_fov(); // inicializace FoV
	return 1;
}

// herní smyčka
void play_game() {
	bool was_said = false;
	while (!TCODConsole::isWindowClosed()) {

		if (!was_said) { // pokud to hra ještě neřekla
			was_said = true;
			if (game_state == "victory") {
				message("You are victorious!",TCODColor::red); // hráč vyhrál
			} else if (game_state == "dead") {
				message("You died!",TCODColor::red); // hráč prohrál
			} else was_said = false; // jinak není co říct
		}

		// pokud jsou všechny příšerky mrtvé
		if (game_state == "playing" && total_monsters == 0) { // hráč vyhrál
			game_state = "victory";
		}

		player_action = "didnt-take-turn"; // hráč zatím netáhl

		handle_keys();

		if (fov_recompute) { // je třeba přepočítat field of view?
			fov_recompute = false;
			fov_map.computeFov(player->x, player->y, TORCH_RADIUS, FOV_LIGHT_WALLS, FOV_ALGO);
		}

		if (game_state == "playing" && player_action != "didnt-take-turn") { // pokud hráč táhl, táhnou všechny příšerky
			for (Object **it = objects.begin(); it != objects.end(); it++) {
				if ((*it)->ai != 0) (*it)->ai->take_turn();
			}
		}

		render_all(); // všechno vykreslit
	}
	save_game(); // po ukončení hry uložit
}

// inicializace nové hry
void new_game() {
	make_map(); // vytvoří mapu
	initialize_fov(); // vytvoří FoV

	// příjemná uvítací zpráva
	message("Welcome, stranger! Prepare to meet your doom!",TCODColor::red);
	//inventory.push(new Object(0,0,'#',"scroll of lightning bolt",TCODColor::lightYellow,false,0,0,new Item(cast_lightning)));
	//inventory.push(new Object(0,0,'#',"scroll of confusion",TCODColor::lightYellow,false,0,0,new Item(cast_confuse)));
	//inventory.push(new Object(0,0,'#',"scroll of fireball",TCODColor::lightYellow,false,0,0,new Item(cast_fireball)));
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
		} else if (choice == 1) { // načíst
			if (load_game() == -1) {
				msg_box("No saved game to load.",24); // není co načíst
			} else {
				play_game();
			}
		} else if (choice == 2) { // ukončit
			break;
		}
	}
}

int main(int argc, char* argv[]) {
	// inicializace
	if(argc == 1) TCODConsole::setCustomFont("terminal10x10_gs_tc.png",TCOD_FONT_LAYOUT_TCOD|TCOD_FONT_TYPE_GREYSCALE);
	TCODConsole::initRoot(SCREEN_WIDTH,SCREEN_HEIGHT,"YARL",false);
	TCODSystem::setFps(FPS_LIMIT);
	TCODConsole::disableKeyboardRepeat();

	con->setBackgroundColor(TCODColor::black);

	main_menu();

	return 0;
}