#include "libtcod.hpp"
#include "SDL.h"
#include <string>
#include <sstream>
#include <cmath>

using namespace std;

// konstanty pro zobrazov�n�
const int SCREEN_WIDTH = 80;
const int SCREEN_HEIGHT = 50;
const int FPS_LIMIT = 20;

// konstanty pro velikost a generov�n� bludi�t�
const int MAP_WIDTH = 80;
const int MAP_HEIGHT = 45;

const int ROOM_MAX_SIZE = 10;
const int ROOM_MIN_SIZE = 6;
const int MAX_ROOMS = 30;

const int MAX_ROOM_MONSTERS = 3;
const int MAX_ROOM_ITEMS = 2;

// konstanty pro u�ivatelsk� rozhran�
const int BAR_WIDTH = 20;
const int PANEL_HEIGHT = 7;
const int PANEL_Y = SCREEN_HEIGHT - PANEL_HEIGHT;

const int MSG_X = BAR_WIDTH + 2;
const int MSG_WIDTH = SCREEN_WIDTH - BAR_WIDTH - 1;
const int MSG_HEIGHT = PANEL_HEIGHT - 1;

const int INVENTORY_WIDTH = 50;

// konstanty pro zobrazov�n� bludi�t�
TCODColor color_dark_ground(0,0,50);
TCODColor color_light_ground(0,0,100);
TCODColor color_dark_wall(50,50,100);
TCODColor color_light_wall(50,50,150);

const TCOD_fov_algorithm_t FOV_ALGO = FOV_BASIC;
const bool FOV_LIGHT_WALLS = true;
const int TORCH_RADIUS = 10;

// konstanty pro p�edm�ty
const int HEAL_AMOUNT = 4;
const int LIGHTNING_RANGE = 5;
const int LIGHTNING_DAMAGE = 20;
const int FIREBALL_RADIUS = 3;
const int FIREBALL_DAMAGE = 12;
const int CONFUSE_RANGE = 8;
const int CONFUSE_NUM_TURNS = 10;

// deklarace nejd�le�it�j��ch t��d
class Object;
class Fighter;
class AI;
class Item;

// string + barva
struct Message {
	string str;
	TCODColor col;
	bool operator==(const Message &m); // oper�tor== vy�adovan� t��dou TCODList
};

bool Message::operator==(const Message &m) {
	return (str == m.str && col == m.col);
}

// entita v bludi�ti
class Object {
public:
	char chr; // znak pro zobrazen�
	TCODColor color; // barva
	int x,y; // poloha
	string name; // jm�no
	bool blocks; // znemo��uje pr�chod?
	Fighter *fighter; // komponenty
	AI *ai;
	Item *item;
	Object(int x_, int y_, char chr_, string name_, const TCODColor & color_, bool blocks_=false,
		Fighter *fighter_component=0, AI *ai_component=0, Item *item_component=0);
	void move(int dx, int dy); // popoj�t o (dx,dy)
	void move_towards(int tgt_x, int tgt_y); // popoj�t sm�rem k (tgt_x,tgt_y)
	void send_to_back(); // poslat na pozad�
	float distance_to(Object &other); // vzd�lenost k jin�mu objektu
	float distance_to(int tgt_x, int tgt_y); // vzd�lenost k (tgt_x,tgt_y)
	void draw(); // vykreslen�
};

// komponenta pro entity, kter� m��ou bojovat
class Fighter {
public:
	Object* owner; // odkaz na vlastn�ka
	void (*death)(Object&); // funkce spu�t�n� p�i smrti
	int hp, max_hp, def, pow; // vlastnosti
	Fighter(int hp, int def, int pow, void (*death_function)(Object&) = 0):
		hp(hp), max_hp(hp), def(def), pow(pow), death(death_function) {;}
	void take_damage(int dmg); // obdr�en� damage
	void attack(Object &target); // �tok na entitu target
	void heal(int amount); // ozdraven�
};

// komponenta pro entity, kter� samy p�em��l�
// abstraktn� t��da
class AI {
public:
	Object* owner; // odkaz na vlastn�ka
	virtual void take_turn() = 0;
	virtual string save() = 0;
};

// z�kladn� p��erka, jen jde za hr��em
class BasicMonster: public AI {
public:
	virtual void take_turn();
	virtual string save();
};

// zmaten� p��erka, chod� n�hodn�
class ConfusedMonster: public AI {
public:
	AI *old_ai; // AI, kter� p��erka m�la p�vodn�
	int num_turns; // po�et kol, po kter� p��erka je�t� bude zmaten�
	ConfusedMonster(AI *old_ai, int num_turns = CONFUSE_NUM_TURNS): old_ai(old_ai), num_turns(num_turns) {}
	virtual void take_turn();
	virtual string save();
};

// komponenta pro entity, kter� lze p�idat do invent��e
class Item {
public:
	Object *owner; // odkaz na vlastn�ka
	int (*use_function)(); // samotn� funkce p�edm�tu
	Item(int (*use_function)() = 0): use_function(use_function) {}
	void pick_up(); // p�id�n� p�edm�tu z bludi�t� do invent��e
	void use(); // pou�it� p�edm�tu
	void drop(); // vr�cen� p�edm�tu z invent��e do bludi�t�
};

// pol��ko v bludi�ti
class Tile {
public:
	bool blocked, blocks_sight, explored; // br�n� v pr�chodu; nen� pr�hledn�; hr�� jej u� vid�l
	Tile(bool blocked = true): blocked(blocked), blocks_sight(blocked), explored(false) {;}
	Tile(bool blocked, bool blocks_sight): blocked(blocked), blocks_sight(blocks_sight), explored(false) {;}
};

// pomocn� t��da pro obd�ln�k
class Rect {
public:
	int x1,x2,y1,y2,center_x,center_y;
	Rect(int x = 0, int y = 0, int w = 0, int h = 0):
		x1(x), y1(y), x2(x+w), y2(y+h), center_x(x + w/2), center_y(y + h/2) {;}
	bool intersect(Rect &other); // prot�n� obd�ln�k n�jak� jin�?
};

bool isBlocked(int x, int y); // je na pol��ku (x,y) n�co, co br�n� v pr�chodu?
string numToStr(int); // pomocn� funkce pro konverzi ��sel na string
string numToStr(float);
string numToStr(double);
void target_tile(int &tgt_x, int &tgt_y, int max_range = -1); // GUI: vybrat pol��ko
Object* target_monster(int max_range = -1); // GUI: vybrat p��erku
void message(string new_msg,const TCODColor& color = TCODColor::white); // GUI: d�t zpr�vu do msgboxu
string capitalize(string &s); // pomocn� funkce pro velk� prvn� p�smeno
float round(float _x); // pomocn� funkce pro zaokrouhlov�n�
double round(double _x);
void render_all(); // vykreslen� v�eho na obrazovku

Tile** map; // bludi�t�
Object* player; // hr��
TCODList<Object*> inventory; // invent��
TCODMap fov_map(MAP_WIDTH,MAP_HEIGHT); // dat. struktura pro field of view
TCODList<Object*> objects; // entity v bludi�ti
TCODList<Message> game_msgs; // GUI: msgbox
TCODConsole *con = new TCODConsole(SCREEN_WIDTH,SCREEN_HEIGHT); // bludi�t�
TCODConsole *panel = new TCODConsole(SCREEN_WIDTH,PANEL_HEIGHT); // informa�n� panel
TCODRandom *rng = TCODRandom::getInstance(); // gener�tor n�h. ��sel
string game_state = "playing"; // stav hry
string player_action = " "; // hr��ova akce
bool fov_recompute = true; // je pot�eba p�epo��tat field of view?
int total_monsters; // celkov� po�et p��erek v bludi�ti

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
	if (fighter != 0) fighter->owner = this; // nastaven� vlastn�ka komponent
	if (ai != 0) ai->owner = this;
	if (item != 0) item->owner = this;
}

// popoj�t o (dx,dy)
void Object::move(int dx, int dy) {
	if (!isBlocked(x+dx,y+dy)) { // lze-li proj�t do c�le
		x += dx;
		y += dy;
	}
}

// popoj�t k pol��ku (tgt_x,tgt_y)
void Object::move_towards(int tgt_x, int tgt_y) {
	int dx = tgt_x - x; // (dx,dy) sm�rov� vektor k c�li
	int dy = tgt_y - y;
	float distance = sqrtf(dx*dx + dy*dy);
	dx = int(round(float(dx)/distance)); // normalizace vektoru
	dy = int(round(float(dy)/distance));
	move(dx,dy);
}

// spo�te vzd�lenost k entit�
float Object::distance_to(Object &other) {
	int dx = other.x - x;
	int dy = other.y - y;
	return sqrtf(dx*dx + dy*dy);
}

// spo�te vzd�lenost k pol��ku
float Object::distance_to(int tgt_x, int tgt_y) {
	int dx = tgt_x - x;
	int dy = tgt_y - y;
	return sqrtf(dx*dx + dy*dy);
}

// po�le entitu na za��tek vykreslovac�ho seznamu
// (bude vykreslen� prvn� -> bude na pozad�)
void Object::send_to_back() {
	objects.remove(this);
	objects.insertBefore(this,0);
}

// vykreslen�
void Object::draw() {
	if (fov_map.isInFov(x,y)) { // je-li entita viditeln�
		con->setFore(x,y,color);
		con->setChar(x,y,int(chr));
	}
}

bool Rect::intersect(Rect &other) {
	return (x1 <= other.x2 && x2 >= other.x1 && y1 <= other.y2 && y2 >= other.y1);
}

// zran�n� bojovn�ka
void Fighter::take_damage(int dmg) {
	if (dmg > 0) hp -= dmg;
	if (hp <= 0 && death != 0) death(*owner); // je-li hp < 0, spustit funkci pro �mrt�
}

// za�to�it na entitu
void Fighter::attack(Object &target) {
	int damage = pow - target.fighter->def; // spo��st dmg
	if (damage > 0) { // je-li v�bec n�jak�
		message(capitalize(owner->name) + " attacks " + target.name + " for " + numToStr(damage) + " hit points.");
		target.fighter->take_damage(damage); // zranit entitu
	} else { // jinak
		message(capitalize(owner->name) + " attacks " + target.name + " but it has no effect.");
	}
}

// vyl��it
void Fighter::heal(int amount) {
	hp += amount;
	if (hp > max_hp) hp = max_hp; // nelze vyl��it nad max_hp
}

// z�kladn� AI p��erky
void BasicMonster::take_turn() {
	if (fov_map.isInFov(owner->x, owner->y)) { // je-li p��erka viditeln� hr��em (<==> p��erka vid� hr��e)
		if (owner->distance_to(*player) >= 2.0) { // je-li d�l ne� 2
			owner->move_towards(player->x, player->y); // j�t k hr��i
		} else if(player->fighter->hp > 0) {
			owner->fighter->attack(*player); // jinak za�to�it na hr��e
		}
	}
}

// v�pis
string BasicMonster::save() {
	return "b";
}

// AI zmaten� p��erky
void ConfusedMonster::take_turn() {
	if (num_turns > 0) { // zb�v�-li n�jak� zmaten�
		owner->move(rng->getInt(-1,1),rng->getInt(-1,1)); // n�hodn� popoj�t
		num_turns -= 1; // posunout po��tadlo
	} else { // jinak
		owner->ai = old_ai; // vr�tit p�vodn� AI
		message("The " + owner->name + " is no longer confused!", TCODColor::red);
		delete this; // tohle u� nepot�ebujeme
	}
}

// v�pis
string ConfusedMonster::save() {
	return "c" + numToStr(num_turns) + old_ai->save(); // zap�e "c", po�et zb�vaj�c�ch kol a vyp�e old_ai
}

// sebrat p�edm�t
void Item::pick_up() {
	if (inventory.size() >= 26) { // nelze m�t v�ce ne� 26 p�edm�t� v invent��i
		message("Your inventory is full, cannot pick up " + owner->name + ".", TCODColor::red);
	} else {
		inventory.push(owner); // p�idat p�edm�t do invent��e
		objects.remove(owner); // odebrat p�edm�t z bludi�t�
		message("You picked up a " + owner->name + "!", TCODColor::green);
	}
}

// pou��t p�edm�t
void Item::use() {
	if (use_function == 0) { // pokud p�edm�t nelze pou��t
		message("The " + owner->name + " cannot be used.");
	} else { // jinak
		if (use_function() != -1) { // pokud pou�it� dopadlo �sp�n�
			inventory.remove(owner); // odebrat p�edm�t z invent��e
		}
	}
}

// zahodit p�edm�t
void Item::drop() {
	objects.push(owner); // p�idat p�edm�t do bludi�t�
	inventory.remove(owner); // odebrat p�edm�t z invent��e
	owner->x = player->x; // p�esunout p�edm�t pod hr��e
	owner->y = player->y;
	message("You dropped a " + owner->name + ".",TCODColor::yellow);
}

// pomocn� funkce
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

// br�n� na pol��ku (x,y) n�co v pr�chodu?
bool isBlocked(int x, int y) {
	if (map[x][y].blocked) return true; // v pr�chodu br�n� bludi�t�
	for (Object** it = objects.begin(); it < objects.end(); it++) {
		if ((*it)->blocks && (*it)->x == x && (*it)->y == y) return true; // v pr�chodu br�n� entita
	}
	return false; // v pr�chodu nebr�n� nic
}

// vr�t� nejbli��� viditelnou p��erku bli��� ne� je range
Object* closest_monster(int range) {
	Object* closest_enemy = 0; // nejbli��� zat�m nalezen� p��erka
	int closest_dist = range + 1; // jej� vzd�lenost k hr��i

	for (Object **it = objects.begin(); it < objects.end(); it++) { // pro v�echny viditeln� p��erky
		if((*it)->fighter != 0 && (*it) != player && fov_map.isInFov((*it)->x,(*it)->y)) {
			float dist = player->distance_to(**it);
			if (dist < closest_dist) { // je-li p��erka bli��� ne� zat�m nejbli��� nalezen�
				closest_enemy = (*it); // je p��erka nov� nejbli��� nalezen�
				closest_dist = dist;
			}
		}
	}
	return closest_enemy;
}

// zalamov�n� ��dk�
TCODList<Message> text_wrap(Message &m, int width) {
	TCODList<Message> ret;
	size_t found = 0;
	while (1) {
		if (m.str.length() < width) { // zbytek se vejde do ��dku
			ret.push(m);
			break;
		}
		found = m.str.find_last_of(" ",width); // naj�t m�sto zalomen�
		if (found == string::npos) found = width;
		Message new_m = {m.str.substr(0,found),m.col}; // zalomit
		ret.push(new_m);
		m.str = m.str.substr(found+1); // se zbytkem ud�lat tot�
	}
	return ret;
}

// GUI: vybrat viditeln� pol��ko bli��� ne� max_range (sou�adnice se zap�� do tgt_x a tgt_y)
// max_range == -1 -> max_range je nekone�no
void target_tile(int &tgt_x, int &tgt_y, int max_range) {
	while(1) {
		render_all(); // vykresl�me v�echno (tj. odstran�me invent��)
		TCODConsole::flush();

		TCOD_key_t key = TCODConsole::checkForKeypress();
		TCOD_mouse_t mouse = TCODMouse::getStatus();
		if (key.vk == TCODK_ENTER && key.lalt) { // alt+enter = p�epnout fullscreen
			TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
		}
		if (mouse.lbutton_pressed && fov_map.isInFov(mouse.cx,mouse.cy) &&
			(max_range == -1 || player->distance_to(mouse.cx,mouse.cy) < max_range)) {
			// hr�� kliknul na viditeln� pol��ko, kter� je bl�e ne� max_range
			tgt_x = mouse.cx; // vr�tit sou�adnice pol��ka
			tgt_y = mouse.cy;
			return;
		}
		if (mouse.rbutton_pressed || key.vk == TCODK_ESCAPE) { // zru�it v�b�r
			tgt_x = -1; // vr�tit neplatnou hodnotu
			tgt_y = -1;
			return;
		}
	}
}

// GUI: vybrat viditelnou p��erku bli��� ne� max_range
// max_range == -1 -> max_range je nekone�no
Object* target_monster(int max_range) {
	while(1) {
		int x,y;
		target_tile(x,y,max_range);
		if (x == -1) { // hr�� zru�il v�b�r
			return 0;
		}
		for (Object **it = objects.begin(); it < objects.end(); it++) {
			if((*it)->x == x && (*it)->y == y && (*it)->fighter != 0 && (*it) != player) {
				// pokud hr�� kliknul na p��erku, vr�t�me ji
				return (*it);
			}
		}
	}
}

// GUI: vyp�e zpr�vu do msgboxu
void message(string new_msg, const TCODColor &color) {
	Message m = {new_msg,color};
	game_msgs.addAll(text_wrap(m,MSG_WIDTH)); // p�idat zalomenou zpr�vu do seznamu
	while(game_msgs.size() > MSG_HEIGHT) { // pokud je seznam moc dlouh�, zkr�tit
		game_msgs.remove(game_msgs.get(0));
	}
}

// GUI: zobraz� menu s hlavi�kou header, mo�nostmi options a ���kou width
int menu(string header, TCODList<string> &options, int width) {
	int header_height = con->getHeightLeftRect(0,0,width,0,header.c_str()); // po�et ��dk� hlavi�ky
	if (header == "") header_height = 0;
	int height = options.size() + header_height + 2; // v��ka menu
	TCODConsole *window = new TCODConsole(width,height); // pomocn� konzole pro menu
	window->setForegroundColor(TCODColor::white);
	window->printLeftRect(0,0,width,0,TCOD_BKGND_NONE,header.c_str());
	int y = header_height + 1;
	char letter_index = 'a'; // za�neme u 'a'
	for (string* it = options.begin(); it < options.end(); it++) { // vyp�e v�echny mo�nosti
		string letter(1,letter_index);
		string text = "(" + letter + ") " + (*it); // v�etn� p�smenka, kter� je pot�eba stisknout
		window->printLeft(0,y,TCOD_BKGND_NONE,text.c_str());
		y += 1;
		letter_index += 1;
	}
	int x = SCREEN_WIDTH/2 - width/2;
	y = SCREEN_HEIGHT/2 - height/2;
	TCODConsole::blit(window, 0, 0, width, height, TCODConsole::root, x, y, 1.0, 0.7); // vykresl� menu
	TCODConsole::root->flush();
	TCOD_key_t key = TCODConsole::waitForKeypress(true); // �ek� na volbu
	if (key.vk == TCODK_ENTER && key.lalt) { // alt+enter = p�epnout fullscreen
		TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
	}
	int index = int(key.c - 'a');
	if (index >= 0 && index < options.size()) return index; // je-li volba platn�, vr�t� ji
	return -1; // jinak vr�t� -1
}

// GUI: zobraz� zpr�vu
void msg_box(string text, int width) {
	TCODList<string> o;
	menu(text, o, width);
}

// vyp�e invent�� a vr�t� hr��em vybran� p�edm�t
Item* inventory_menu(string header) {
	TCODList<string> options;
	if (inventory.size() == 0) { // je-li invent�� pr�zdn�
		options.push("Inventory is empty."); // upozorn� hr��e
	} else { // jinak
		for (Object **it = inventory.begin(); it < inventory.end(); it++) {
			options.push((*it)->name); // vyp�e invent��
		}
	}
	int index = menu(header, options, INVENTORY_WIDTH); // nech� hr��e vybrat
	if (index == -1 || inventory.size() == 0) return 0; // je-li volba neplatn�, nevr�t� ��dn� p�edm�t
	return inventory.get(index)->item; // jinak vr�t� vybran� p�edm�t
}

// funkce pro uzdravovac� lektvar
int cast_heal() {
	if (player->fighter->hp == player->fighter->max_hp) { // je-li hr�� zdrav�
		message("You are already at full health.", TCODColor::red);
		return -1; // lektvar nebyl pou�it
	}
	message("Your wounds start to feel better.", TCODColor::violet);
	player->fighter->heal(HEAL_AMOUNT); // uzdrav� hr��e
	return 1; // vr�t� �sp�ch
}

// funkce pro svitek blesku
int cast_lightning() {
	Object* monster = closest_monster(LIGHTNING_RANGE); // zam��� na nejbli��� p��erku v dosahu
	if (monster == 0) { // pokud nen� ��dn� v dosahu
		message("No monster is close enough to strike.", TCODColor::red);
		return -1; // svitek nebyl pou�it
	}
	message("A lightning bolt strikes the " + monster->name + " for " + numToStr(LIGHTNING_DAMAGE) + " hit points.",TCODColor::lightBlue);
	monster->fighter->take_damage(LIGHTNING_DAMAGE); // poran� p��erku
	return 1; // vr�t� �sp�ch
}

// funkce pro svitek ohniv� koule
int cast_fireball() {
	message("Left-click a target tile for the fireball, or right-click to cancel.", TCODColor::lightCyan);
	int x, y;
	target_tile(x,y); // nech� hr��e vybrat c�l
	if (x == -1) { // pokud nebyl vybr�n platn� c�l
		message("Spell cancelled.", TCODColor::cyan);
		return -1; // svitek nebyl pou�it
	}
	message("The fireball explodes, burning everything within " + numToStr(FIREBALL_RADIUS) + " tiles!", TCODColor::orange);

	for (Object **it = objects.begin(); it < objects.end(); it++) { // poran� v�echny p��erky (a hr��e) v dosahu
		if ((*it)->distance_to(x,y) <= FIREBALL_RADIUS && (*it)->fighter != 0) {
			message("The " + (*it)->name + " gets burned for " + numToStr(FIREBALL_DAMAGE) + " hit points.", TCODColor::orange);
			(*it)->fighter->take_damage(FIREBALL_DAMAGE);
		}
	}
	return 1;
}

// funkce pro svitek om�men�
int cast_confuse() {
	message("Left-click an enemy to confuse it, or right-click to cancel.", TCODColor::lightCyan);
	Object *monster = target_monster(CONFUSE_RANGE); // nech� hr��e vybrat c�l
	if (monster == 0) { // pokud nebyl vybr�n c�l
		message("Spell cancelled.", TCODColor::cyan);
		return -1; // svitek nebyl pou�it
	}

	AI* old_ai = monster->ai;
	monster->ai = new ConfusedMonster(old_ai); // AI p��erky se nahrad� zmaten�m AI
	monster->ai->owner = monster;
	message("The eyes of the " + monster->name + " glaze over, as it starts to stumble around!", TCODColor::orange);
	return 1;
}

// vr�t� jm�na entit pod kurzorem, odd�len� ��rkami a s prvn�m p�smenem velk�m
string get_names_under_mouse() {
	TCOD_mouse_t mouse = TCODMouse::getStatus();
	string ret;
	for (Object **it = objects.begin(); it < objects.end(); it++) {
		// p�id� do stringu ret jm�na v�ech entit pod kurzorem
		if ((*it)->x == mouse.cx && (*it)->y == mouse.cy && fov_map.isInFov((*it)->x,(*it)->y)) {
			ret += (*it)->name + ", ";
		}
	}
	if (ret.length() > 0) ret.erase(ret.end()-2,ret.end()); // uma�eme posledn� ��rku
	return capitalize(ret);
}

// funkce pro smrt hr��e
void player_death(Object& plr) {
	game_state = "dead"; // nastav� stav hry
	plr.chr = '%'; // prom�n� hr��e v mrtvolu
	plr.color = TCODColor::darkRed;
}

// funkce pro smrt p��erky
void monster_death(Object& monster) {
	monster.chr = '%'; // prom�n� p��erku v mrtvolu
	monster.color = TCODColor::darkRed;
	monster.blocks = false; // p�es mrtvolu lze proch�zet
	delete monster.fighter; // s mrtvolou nelze bojovat
	monster.fighter = 0;
	delete monster.ai; // mrtvola nemysl�
	monster.ai = 0;
	message(capitalize(monster.name) + " is dead!",TCODColor::orange);
	monster.name = "remains of " + monster.name; // zm�n� jm�no
	monster.send_to_back(); // vr�t� na konec
	total_monsters -= 1;
}

// funkce pro pohyb/�tok hr��e
void player_move_or_attack(int dx, int dy) {
	int x = player->x + dx;
	int y = player->y + dy;

	Object* target = 0;
	for (Object ** it = objects.begin(); it < objects.end(); it++) {
		// vrazil hr�� do p��erky?
		if ((*it)->fighter != 0 && (*it)->x == x && (*it)->y == y) {
			target = (*it);
			break;
		}
	}

	if (target != 0) { // pokud ano, tak na ni za�to�il
		player_action = "attack";
		player->fighter->attack(*target);
	} else { // jinak se jen pohnul
		player->move(dx,dy);
		player_action = "move";
		fov_recompute = true; // pak je pot�eba p�epo��tat FoV
	}
}

// vygeneruje m�stnost v bludi�ti
void create_room(Rect &room) {
	for (int i = room.x1 + 1; i < room.x2; i++) {
		for (int j = room.y1 + 1; j < room.y2; j++) {
			map[i][j].blocked = false;
			map[i][j].blocks_sight = false;
		}
	}
}

// vygeneruje horizont�ln� tunel v bludi�ti
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

// vygeneruje vertik�ln� tunel v bludi�ti
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

// zabydl� m�stnost
void place_objects(Rect &room) {
	// vygeneruje p��erky
	int num_monsters = rng->getInt(0,MAX_ROOM_MONSTERS);
	for (int i = 0; i < num_monsters; i++) {
		int x = rng->getInt(room.x1 + 1,room.x2 - 1); // um�st�n�
		int y = rng->getInt(room.y1 + 1,room.y2 - 1);
		int choice = rng->getInt(0,100); // v�b�r typu
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
	// vygeneruje p�edm�ty
	int num_items = rng->getInt(0,MAX_ROOM_ITEMS);
	for (int i = 0; i < num_items; i++) {
		int x = rng->getInt(room.x1 + 1,room.x2 - 1);
		int y = rng->getInt(room.y1 + 1,room.y2 - 1);
		if (!isBlocked(x,y)) { // je-li m�sto voln�
			int choice = rng->getInt(0,100); // v�b�r typu
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
	for (int i = 0; i < MAP_WIDTH; i++) { // vygenerov�n� mapy pro FoV
		for (int j = 0; j < MAP_HEIGHT; j++) {
			fov_map.setProperties(i,j,!map[i][j].blocked,!map[i][j].blocks_sight);
		}
	}
}

// vygeneruje bludi�t�
void make_map() {
	player = new Object(0, 0, '@', "player", TCODColor::white, true, new Fighter(30,2,5,player_death));
	objects.push(player);

	map = new Tile*[MAP_WIDTH]; // nap�ed je v�e pr�zdn�
	for (int i = 0; i < MAP_WIDTH; i++) {
		map[i] = new Tile[MAP_HEIGHT];
	}

	TCODList<Rect> rooms;
	int num_rooms = 0;

	// vygeneruje m�stnosti
	for (int i = 0; i < MAX_ROOMS; i++) {
		int w = rng->getInt(ROOM_MIN_SIZE, ROOM_MAX_SIZE);
		int h = rng->getInt(ROOM_MIN_SIZE, ROOM_MAX_SIZE);
		int x = rng->getInt(0, MAP_WIDTH - w - 2);
		int y = rng->getInt(0, MAP_HEIGHT - h - 2);
		Rect new_room(x,y,w,h);
		bool failed = false; // prot�n� se nov� m�stnost s u� n�jakou vygenerovanou?
		for (Rect* it = rooms.begin(); it < rooms.end(); it++) {
			if (new_room.intersect(*it)) {
				failed = true;
				break;
			}
		}
		if (!failed) { // pokud ne, p�id� se do mapy
			create_room(new_room);
			int new_x = new_room.center_x;
			int new_y = new_room.center_y;
			if (num_rooms == 0) { // je-li to prvn� m�stnost, bude v n� hr��
				player->x = new_x;
				player->y = new_y;
			} else { // jinak se spoj� s p�edchoz� vygenerovanou m�stnost� tunely
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
			place_objects(new_room); // zabydl� novou m�stnost
			num_rooms += 1;
		}
	}
}

// GUI: vykresl� ukazatel hodnoty na sou�adnic�ch (x,y) o dan�ch barv�ch
void render_bar(int x, int y, int total_width, string name, int value, int maximum,
				const TCODColor &bar_color, const TCODColor &back_color) {
	int bar_width = int(float(total_width*value)/maximum); // pozad�
	panel->setBackgroundColor(back_color);
	panel->rect(x,y,total_width,1,false);
	panel->setBackgroundColor(bar_color);
	if (bar_width > 0) panel->rect(x,y,bar_width,1,false); // pop�ed�
	panel->setForegroundColor(TCODColor::white); // text
	panel->printCenter(x + total_width/2, y, TCOD_BKGND_NONE, "%s: %d/%d", name.c_str(), value, maximum);
}

// vykreslit v�echno
void render_all() {
	TCODConsole::root->clear(); // v�echno vyh�zet
	con->clear();

	for (Object** it = objects.begin(); it != objects.end(); it++) { // vykreslit entity
		(*it)->draw();
	}
	player->draw(); // hr��e vykreslit navrch

	for (int i = 0; i < MAP_WIDTH; i++) { // vykreslit bludi�t�
		for (int j = 0; j < MAP_HEIGHT; j++) {
			bool wall = map[i][j].blocks_sight;
			bool visible = fov_map.isInFov(i,j);
			if(visible) { // pol��ko je vid�t
				map[i][j].explored = true; // pak ho hr�� prozkoumal
				if(wall) con->setBack(i,j,color_light_wall);
				else con->setBack(i,j,color_light_ground);				
			} else { // pol��ko nen� vid�t
				if (map[i][j].explored) { // vykresl�me jej jen pokud u� ho hr�� prozkoumal
					if(wall) con->setBack(i,j,color_dark_wall);
					else con->setBack(i,j,color_dark_ground);
				}
			}
		}
	}

	// vykreslen� panelu
	panel->setBackgroundColor(TCODColor::black);
	panel->clear();

	// ukazatel HP
	render_bar(1, 1, BAR_WIDTH, "HP", player->fighter->hp, player->fighter->max_hp,
		TCODColor::lightRed, TCODColor::darkerRed);

	panel->setForegroundColor(TCODColor::white);
	int y = 1;
	for (Message* it = game_msgs.begin(); it < game_msgs.end(); it++) { // vypsat zpr�vy z msgboxu
		panel->setForegroundColor((*it).col);
		panel->printLeft(MSG_X,y,TCOD_BKGND_NONE,(*it).str.c_str());
		y += 1;
	}

	panel->setForegroundColor(TCODColor::grey);
	panel->printLeft(1,0,TCOD_BKGND_NONE,get_names_under_mouse().c_str()); // vypsat vysv�tlivky

	// v�e vykreslit na obrazovku
	TCODConsole::blit(con, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TCODConsole::root, 0, 0);
	TCODConsole::blit(panel, 0, 0, SCREEN_WIDTH, PANEL_HEIGHT, TCODConsole::root, 0, PANEL_Y);
	TCODConsole::flush();
}

// reaguje na kl�vesy
void handle_keys() {
	TCOD_key_t key = TCODConsole::checkForKeypress(TCOD_KEY_PRESSED); // kl�vesa
	if (game_state == "playing") { // pokud hra prob�h�
		if (key.vk == TCODK_UP) { // nahoru
			if (player->y > 0) player_move_or_attack(0,-1);
		} else if (key.vk == TCODK_DOWN) { // dolu
			if (player->y < SCREEN_HEIGHT - 1) player_move_or_attack(0,1);
		} else if (key.vk == TCODK_LEFT) { // doleva
			if (player->x > 0) player_move_or_attack(-1,0);
		} else if (key.vk == TCODK_RIGHT) { // doprava
			if (player->x < SCREEN_WIDTH - 1) player_move_or_attack(1,0);
		} else if (key.c == 'g') { // sebrat p�edm�t (pokud pod hr��em n�jak� je)
			for (Object **it = objects.begin(); it < objects.end(); it++) {
				if ((*it)->x == player->x && (*it)->y == player->y && (*it)->item != 0) {
					(*it)->item->pick_up();
					break;
				}
			}
		} else if (key.c == 'i') { // pou��t p�edm�t z invent��e
			Item* chosen_item = inventory_menu("Press the key next to an item to use it, or any other to cancel.");
			if (chosen_item != 0) { // pokud hr�� vybral p�edm�t, pou��t ho
				chosen_item->use();
			}
		} else if (key.c == 'd') { // vyhodit p�edm�t z invent��e
			Item* chosen_item = inventory_menu("Press the key next to an item to drop it, or any other to cancel.");
			if (chosen_item != 0) { // pokud hr�� vybral p�edm�t, polo�it ho
				chosen_item->drop();
			}
		}
	}
	if (key.vk == TCODK_ENTER && key.lalt) { // alt+enter = p�epnout fullscreen
		TCODConsole::setFullscreen(!TCODConsole::isFullscreen());
	}
}

// ulo�� hru
void save_game() {
	TCODZip zip; // buffer
	for (int i = 0; i < MAP_WIDTH; i++) for (int j = 0; j < MAP_HEIGHT; j++) { // ulo�it bludi�t�
		zip.putInt(map[i][j].blocked);
		zip.putInt(map[i][j].blocks_sight);
		zip.putInt(map[i][j].explored);
	}
	zip.putInt(total_monsters); // ulo�it po�et p��erek
	zip.putInt(objects.size()); // po�et entit
	int player_index = 0;
	int i = 0;
	for (Object **it = objects.begin(); it < objects.end(); it++, i++) { // postupn� ulo�� v�echny entity
		if (player==(*it)) player_index = i;
		zip.putInt((*it)->blocks); // data
		zip.putColor(&((*it)->color));
		zip.putChar((*it)->chr);
		zip.putString((*it)->name.c_str());
		zip.putInt((*it)->x);
		zip.putInt((*it)->y);
		if ((*it)->fighter != 0) { // m�-li entita komponentu fighter
			zip.putInt(1); // p��znak
			zip.putInt((*it)->fighter->hp); // data
			zip.putInt((*it)->fighter->max_hp);
			zip.putInt((*it)->fighter->pow);
			zip.putInt((*it)->fighter->def);
			if ((*it)->fighter->death == player_death) { // identifik�tor funkce
				zip.putInt(1);
			} else zip.putInt(0);
		} else zip.putInt(0); // entita nem� komponentu fighter
		if ((*it)->ai != 0) { // m�-li entita komponentu AI
			zip.putInt(1); // p��znak
			zip.putString((*it)->ai->save().c_str()); // AI se ukl�d� samo
		} else zip.putInt(0); // entita nem� komponentu AI
		if ((*it)->item != 0) { // m�-li entita komponentu Item
			zip.putInt(1); // p��znak
		} else zip.putInt(0); // entita nem� komponentu Item
	}
	zip.putInt(player_index); // index hr��e v seznamu entit
	zip.putInt(inventory.size()); // velikost invent��e
	for (Object **it = inventory.begin(); it < inventory.end(); it++) { // postupn� ulo�� cel� invent��
		zip.putInt((*it)->blocks); // data
		zip.putColor(&((*it)->color));
		zip.putChar((*it)->chr);
		zip.putString((*it)->name.c_str());
		zip.putInt((*it)->x);
		zip.putInt((*it)->y);
	}
	zip.putString(game_state.c_str()); // ulo�� stav hry
	zip.saveToFile("savegame"); // ulo�� buffer do souboru savegame
}

// na�te hru
int load_game() {
	TCODZip zip; // buffer
	if (zip.loadFromFile("savegame") == 0) return -1; // nen�-li co na��st, vr�t� -1
	map = new Tile*[MAP_WIDTH]; // na�ten� bludi�t�
	for (int i = 0; i < MAP_WIDTH; i++) {
		map[i] = new Tile[MAP_HEIGHT];
		for (int j = 0; j < MAP_HEIGHT; j++) {
			map[i][j].blocked = zip.getInt();
			map[i][j].blocks_sight = zip.getInt();
			map[i][j].explored = zip.getInt();
		}
	}
	total_monsters = zip.getInt(); // na�ten� po�tu p��erek
	int objects_size = zip.getInt(); // na�ten� po�tu entit
	for (int i = 0; i < objects_size; i++) { // postupn� napln� entitami
		bool blocks = zip.getInt(); // data
		TCODColor color = zip.getColor();
		char chr = zip.getChar();
		string name = zip.getString();
		int x = zip.getInt();
		int y = zip.getInt();
		Object *o = new Object(x,y,chr,name,color,blocks); // vytvo�� entitu
		Fighter *f = 0; // komponenta Fighter
		if (zip.getInt() == 1) { // m�-li
			int hp = zip.getInt(); // na�ten� dat
			int max_hp = zip.getInt();
			int pow = zip.getInt();
			int def = zip.getInt();
			if (zip.getInt() == 1) { // p�i�azen� funkce
				f = new Fighter(max_hp,def,pow,player_death);
			} else {
				f = new Fighter(max_hp,def,pow,monster_death);
			}
			f->hp = hp;
			f->owner = o; // p�i�azen� vlastn�ka
		}
		AI *a = 0; // komponenta AI
		if (zip.getInt() == 1) { // m�-li
			string s = zip.getString(); // zpracov�n� stringu
			a = new BasicMonster(); // zat�m je v�dy na konci BasicMonster
			a->owner = o; // p�i�azen� vlastn�ka
			while (s.length() > 1) { // dokud nejsme na za��tku
				size_t found = s.find_last_of("c"); // mezi posledn�m c a koncem je hodnota
				int n = atoi(s.substr(found+1).c_str()); // tu p�e�teme
				a = new ConfusedMonster(a,n); // p�id�me ConfusedMonster s pat�i�n�mi parametry
				a->owner = o; // p�i�azen� vlastn�ka
				s = s.substr(0,found); // pod�v�me se na zbytek stringu
			}
		}
		Item *it = 0; // komponenta AI
		if (zip.getInt() == 1) { // m�-li
			if (name == "healing potion") { // funkci p�i�ad�me podle jm�na
				it = new Item(cast_heal);
			} else if (name == "scroll of lightning bolt") {
				it = new Item(cast_lightning);
			} else if (name == "scroll of confusion") {
				it = new Item(cast_confuse);
			} else if (name == "scroll of fireball") {
				it = new Item(cast_fireball);
			}
			it->owner = o; // p�i�azen� vlastn�ka
		}
		o->fighter = f; // p�i�azen� komponent
		o->ai = a;
		o->item = it;
		objects.push(o); // za�azen� do seznamu
	}
	player = objects.get(zip.getInt()); // index hr��e
	int inv_size = zip.getInt(); // na�ten� velikosti invent��e
	for (int i = 0; i < inv_size; i++) { // postupn� napln� invent��
		bool blocks = zip.getInt(); // data
		TCODColor color = zip.getColor();
		char chr = zip.getChar();
		string name = zip.getString();
		int x = zip.getInt();
		int y = zip.getInt();
		Item* it = 0; // komponenta Item
		if (name == "healing potion") { // p�i�azen� funkce podle jm�na
			it = new Item(cast_heal);
		} else if (name == "scroll of lightning bolt") {
			it = new Item(cast_lightning);
		} else if (name == "scroll of confusion") {
			it = new Item(cast_confuse);
		} else if (name == "scroll of fireball") {
			it = new Item(cast_fireball);
		}
		inventory.push(new Object(x,y,chr,name,color,blocks,0,0,it)); // za�azen� do invent��e
	}
	game_state = zip.getString(); // na�ten� stavu hry
	initialize_fov(); // inicializace FoV
	return 1;
}

// hern� smy�ka
void play_game() {
	bool was_said = false;
	while (!TCODConsole::isWindowClosed()) {

		if (!was_said) { // pokud to hra je�t� ne�ekla
			was_said = true;
			if (game_state == "victory") {
				message("You are victorious!",TCODColor::red); // hr�� vyhr�l
			} else if (game_state == "dead") {
				message("You died!",TCODColor::red); // hr�� prohr�l
			} else was_said = false; // jinak nen� co ��ct
		}

		// pokud jsou v�echny p��erky mrtv�
		if (game_state == "playing" && total_monsters == 0) { // hr�� vyhr�l
			game_state = "victory";
		}

		player_action = "didnt-take-turn"; // hr�� zat�m net�hl

		handle_keys();

		if (fov_recompute) { // je t�eba p�epo��tat field of view?
			fov_recompute = false;
			fov_map.computeFov(player->x, player->y, TORCH_RADIUS, FOV_LIGHT_WALLS, FOV_ALGO);
		}

		if (game_state == "playing" && player_action != "didnt-take-turn") { // pokud hr�� t�hl, t�hnou v�echny p��erky
			for (Object **it = objects.begin(); it < objects.end(); it++) {
				if ((*it)->ai != 0) (*it)->ai->take_turn();
			}
		}

		render_all(); // v�echno vykreslit
	}
	save_game(); // po ukon�en� hry ulo�it
}

// inicializace nov� hry
void new_game() {
	make_map(); // vytvo�� mapu
	initialize_fov(); // vytvo�� FoV

	// p��jemn� uv�tac� zpr�va
	message("Welcome, stranger! Prepare to meet your doom!",TCODColor::red);
}

// hlavn� menu
void main_menu() {
	TCODList<string> o; // mo�nosti hlavn�ho menu
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
		if (choice == 0) { // nov� hra
			new_game();
			play_game();
		} else if (choice == 1) { // na��st
			if (load_game() == -1) {
				msg_box("No saved game to load.",24); // nen� co na��st
			} else {
				play_game();
			}
		} else if (choice == 2) { // ukon�it
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