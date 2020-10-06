#include "DirectOverlay.h"
#include <D3dx9math.h>
#include <sstream>
#include <string>
#include <algorithm>
#include <list>

#define M_PI 3.14159265358979323846264338327950288419716939937510

#define OFFSET_UWORLD 0x6C0A9F8 // 48 8b 0d ? ? ? ? 48 85 c9 74 30 e8 ? ? ? ? 48 8b f8
#define OFFSET_CAMLOC 0x6BE5070 // F3 44 0F 11 1D ? ? ? ?
#define OFFSET_PLAYERSTATE 0x238
#define OFFSET_TEAMINDEX 0xE20
#define OFFSET_GNAME 0x06849b40
#define OFFSET_SEED 0x067cd7dc
#define OFFSET_GOBJECTS 0x68616D8
#define OFFSET_OBJECTNAMES 0x337E920
#define OFFSET_PROCESSEVENT 0x24D35A0

float aimfov = 25.0f;
const int MAX_SMOOTH_VALUE = 100;



// CHEETOS LIST
bool Menu = false;
bool Aimbot = true;
bool DebugCrap = false;
bool EnemyESP = true;
bool skeleton = false;
bool BoxESP = true;
bool LineESP = false;
bool ItemESP = false;
bool DistanceESP = true;
bool CircleFOV = true;
bool aimpred = false;

DWORD processID;
HWND hwnd = NULL;

int width;
int height;
int localplayerID;

HANDLE Driver_File;
uint64_t base_address;

DWORD_PTR Uworld;
DWORD_PTR LocalPawn;
DWORD_PTR Localplayer;
DWORD_PTR Rootcomp;
DWORD_PTR PlayerController;
DWORD_PTR Ulevel;

Vector3 localactorpos;

FTransform GetBoneIndex(DWORD_PTR mesh, int index)
{
	DWORD_PTR bonearray = read<DWORD_PTR>(Driver_File, processID, mesh + 0x410);

	return read<FTransform>(Driver_File, processID, bonearray + (index * 0x30));
}

Vector3 GetBoneWithRotation(DWORD_PTR mesh, int id)
{
	FTransform bone = GetBoneIndex(mesh, id);
	FTransform ComponentToWorld = read<FTransform>(Driver_File, processID, mesh + 0x1C0);

	D3DMATRIX Matrix;
	Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());

	return Vector3(Matrix._41, Matrix._42, Matrix._43);
}

D3DXMATRIX Matrix(Vector3 rot, Vector3 origin = Vector3(0, 0, 0))
{
	float radPitch = (rot.x * float(M_PI) / 180.f);
	float radYaw = (rot.y * float(M_PI) / 180.f);
	float radRoll = (rot.z * float(M_PI) / 180.f);

	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);

	D3DMATRIX matrix;
	matrix.m[0][0] = CP * CY;
	matrix.m[0][1] = CP * SY;
	matrix.m[0][2] = SP;
	matrix.m[0][3] = 0.f;

	matrix.m[1][0] = SR * SP * CY - CR * SY;
	matrix.m[1][1] = SR * SP * SY + CR * CY;
	matrix.m[1][2] = -SR * CP;
	matrix.m[1][3] = 0.f;

	matrix.m[2][0] = -(CR * SP * CY + SR * SY);
	matrix.m[2][1] = CY * SR - CR * SP * SY;
	matrix.m[2][2] = CR * CP;
	matrix.m[2][3] = 0.f;

	matrix.m[3][0] = origin.x;
	matrix.m[3][1] = origin.y;
	matrix.m[3][2] = origin.z;
	matrix.m[3][3] = 1.f;

	return matrix;
}

Vector3 ProjectWorldToScreen(Vector3 WorldLocation, Vector3 camrot)
{
	Vector3 Screenlocation = Vector3(0, 0, 0);
	Vector3 Rotation = camrot; // FRotator

	D3DMATRIX tempMatrix = Matrix(Rotation);

	Vector3 vAxisX, vAxisY, vAxisZ;

	vAxisX = Vector3(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
	vAxisY = Vector3(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
	vAxisZ = Vector3(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

	Vector3 camloc = read<Vector3>(Driver_File, processID, base_address + OFFSET_CAMLOC);

	Vector3 vDelta = WorldLocation - camloc;
	Vector3 vTransformed = Vector3(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

	if (vTransformed.z < 1.f)
		vTransformed.z = 1.f;

	uint64_t zoomBase = read<uint64_t>(Driver_File, processID, Localplayer + 0xb0);
	float zoom = read<float>(Driver_File, processID, zoomBase + 0x500);

	float FovAngle = 80.0f / (zoom / 1.19f);
	float ScreenCenterX = width / 2.0f;
	float ScreenCenterY = height / 2.0f;

	Screenlocation.x = ScreenCenterX + vTransformed.x * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	Screenlocation.y = ScreenCenterY - vTransformed.y * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;

	return Screenlocation;
}

Vector3 Camera(unsigned __int64 RootComponent)
{
	unsigned __int64 PtrPitch;
	Vector3 Camera;

	auto pitch = read<uintptr_t>(Driver_File, processID, Localplayer + 0xb0);
	Camera.x = read<float>(Driver_File, processID, RootComponent + 0x12C);
	Camera.y = read<float>(Driver_File, processID, pitch + 0x678);

	float test = asin(Camera.y);
	float degrees = test * (180.0 / M_PI);

	Camera.y = degrees;

	if (Camera.x < 0)
		Camera.x = 360 + Camera.x;

	return Camera;
}

void menu()
{
	if (Menu)
	{

		// Draw the fancy stuff OwO
		DrawBox(5.f, 5.f, 200.f, 300.f, 0.f, 0.f, 0.f, 0.f, 0.7f, true);
		DrawBox(3, 3, 204, 302, 2.0f, 255, 255, 255, 100, false);
		DrawLine(3, 40, 206, 40, 2.0f, 255, 255, 255, 100);

		DrawString(_xor_("Paradox FN 1.0").c_str(), 17, 10, 8, 255.f, 255.f, 255.f, 255.f);

		if (EnemyESP)
			DrawString(_xor_("on").c_str(), 13, 10 + 150, 10 + 40, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("off").c_str(), 13, 10 + 150, 10 + 40, 255.f, 0.f, 0.f, 255.f);

		DrawString(_xor_("[F1] Enemy ESP").c_str(), 13, 10, 10 + 40, 255.f, 255.f, 255.f, 255.f);

		if (BoxESP)
			DrawString(_xor_("on").c_str(), 13, 10 + 150, 10 + 60, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("off").c_str(), 13, 10 + 150, 10 + 60, 255.f, 0.f, 0.f, 255.f);

		DrawString(_xor_("[F2] Box ESP").c_str(), 13, 10, 10 + 60, 255.f, 255.f, 255.f, 255.f);

		if (LineESP)
			DrawString(_xor_("on").c_str(), 13, 10 + 150, 10 + 80, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("off").c_str(), 13, 10 + 150, 10 + 80, 255.f, 0.f, 0.f, 255.f);

		DrawString(_xor_("[F3] Line ESP").c_str(), 13, 10, 10 + 80, 255.f, 255.f, 255.f, 255.f);


		if (DistanceESP)
			DrawString(_xor_("on").c_str(), 13, 10 + 150, 10 + 100, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("off").c_str(), 13, 10 + 150, 10 + 100, 255.f, 0.f, 0.f, 255.f);

		DrawString(_xor_("[F4] Distance ESP").c_str(), 13, 10, 10 + 100, 255.f, 255.f, 255.f, 255.f);

		if (skeleton)
			DrawString(_xor_("on").c_str(), 13, 10 + 150, 10 + 120, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("off").c_str(), 13, 10 + 150, 10 + 120, 255.f, 0.f, 0.f, 255.f);

		DrawString(_xor_("[F5] Skeleton ESP").c_str(), 13, 10, 10 + 120, 255.f, 255.f, 255.f, 255.f);

		if (Aimbot)
			DrawString(_xor_("on").c_str(), 13, 10 + 150, 10 + 140, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("off").c_str(), 13, 10 + 150, 10 + 140, 255.f, 0.f, 0.f, 255.f);

		DrawString(_xor_("[F6] Aimbot").c_str(), 13, 10, 10 + 140, 255.f, 255.f, 255.f, 255.f);

		if (CircleFOV)
			DrawString(_xor_("on").c_str(), 13, 10 + 150, 10 + 160, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("off").c_str(), 13, 10 + 150, 10 + 160, 255.f, 0.f, 0.f, 255.f);

		DrawString(_xor_("[F7] FOV Circle").c_str(), 13, 10, 10 + 160, 255.f, 255.f, 255.f, 255.f);

	
			DrawString(_xor_("[U/D] FOVCircle >>").c_str(), 13, 10, 10 + 180, 255.f, 255.f, 255.f, 255.f);
		DrawString((std::to_string(aimfov)).c_str(), 13, 10 + 115, 10 + 180, 255.f, 255.f, 255.f, 255.f);
	
	}
}

DWORD Menuthread(LPVOID in)
{
	while (1)
	{
		if (GetAsyncKeyState(VK_INSERT) & 1) {
			Menu = !Menu;
		}

		if (Menu)
		{
			if (GetAsyncKeyState(VK_F1) & 1) {
				EnemyESP = !EnemyESP;
			}

			if (GetAsyncKeyState(VK_F2) & 1) {
				BoxESP = !BoxESP;
			}

			if (GetAsyncKeyState(VK_F3) & 1) {
				LineESP = !LineESP;
			}
			

			if (GetAsyncKeyState(VK_F4) & 1) {
				DistanceESP = !DistanceESP;
			}

			if (GetAsyncKeyState(VK_F5) & 1) {
				skeleton = !skeleton;
			}

			if (GetAsyncKeyState(VK_F6) & 1) {
				Aimbot = !Aimbot;
			}

			if (GetAsyncKeyState(VK_F7) & 1) {
				CircleFOV = !CircleFOV;
			}	
			if (aimfov) {
				if (GetAsyncKeyState(VK_UP) & 1) {
					aimfov += 1;
				}
			}
			if (aimfov < MAX_SMOOTH_VALUE) {
				if (GetAsyncKeyState(VK_DOWN) & 1) {
					aimfov -= 1;
				}
			}
		}
	}
}

Vector3 AimbotCorrection(float bulletVelocity, float bulletGravity, float targetDistance, Vector3 targetPosition, Vector3 targetVelocity)
{
	Vector3 recalculated = targetPosition;

	float gravity = fabs(bulletGravity);
	float time = targetDistance / fabs(bulletVelocity);

	/* Bullet drop correction */
	float bulletDrop = (gravity / 2) * time * time;
	recalculated.z += bulletDrop * 100;

	/* Player movement correction */
	recalculated.x += time * (targetVelocity.x);
	recalculated.y += time * (targetVelocity.y);
	recalculated.z += time * (targetVelocity.z);

	return recalculated;
}


bool isaimbotting;

uint64_t entityx;

bool GetAimKey()
{
	return (GetAsyncKeyState(VK_RBUTTON)); // AIMB0T KEY
}

void aimbot(float x, float y)
{
	float ScreenCenterX = (width / 2);
	float ScreenCenterY = (height / 2);
	int AimSpeed = 3.0f;
	float TargetX = 0;
	float TargetY = 0;

	if (x != 0)
	{
		if (x > ScreenCenterX)
		{
			TargetX = -(ScreenCenterX - x);
			TargetX /= AimSpeed;
			if (TargetX + ScreenCenterX > ScreenCenterX * 2) TargetX = 0;
		}

		if (x < ScreenCenterX)
		{
			TargetX = x - ScreenCenterX;
			TargetX /= AimSpeed;
			if (TargetX + ScreenCenterX < 0) TargetX = 0;
		}
	}

	if (y != 0)
	{
		if (y > ScreenCenterY)
		{
			TargetY = -(ScreenCenterY - y);
			TargetY /= AimSpeed;
			if (TargetY + ScreenCenterY > ScreenCenterY * 2) TargetY = 0;
		}

		if (y < ScreenCenterY)
		{
			TargetY = y - ScreenCenterY;
			TargetY /= AimSpeed;
			if (TargetY + ScreenCenterY < 0) TargetY = 0;
		}
	}

	mouse_event(MOUSEEVENTF_MOVE, static_cast<DWORD>(TargetX), static_cast<DWORD>(TargetY), NULL, NULL);

	return;
}

double GetCrossDistance(double x1, double y1, double x2, double y2)
{
	return sqrt(pow((x2 - x1), 2) + pow((y2 - y1), 2));
}

bool GetClosestPlayerToCrossHair(Vector3 Pos, float& max, float aimfov, DWORD_PTR entity)
{
	if (!GetAimKey() || !isaimbotting)
	{
		if (entity)
		{
			float Dist = GetCrossDistance(Pos.x, Pos.y, (width / 2), (height / 2));

			if (Dist < max)
			{
				max = Dist;
				entityx = entity;
				aimfov = aimfov;
			}
		}
	}
	return false;
}

void AimAt(DWORD_PTR entity, Vector3 Localcam)
{
	Vector3 rootHeadOut;

	uint64_t currentactormesh = read<uint64_t>(Driver_File, processID, entity + 0x278);
	uint64_t CurrentActorRootComponent = read<uint64_t>(Driver_File, processID, entity + 0x130);

	auto rootHead = GetBoneWithRotation(currentactormesh, 66);

	if (aimpred)
	{
		float distance = localactorpos.Distance(rootHead) / 100.f;
		Vector3 vellocity = read<Vector3>(Driver_File, processID, CurrentActorRootComponent + 0x168);
		Vector3 Predicted = AimbotCorrection(30000, -504, distance, rootHead, vellocity);

		rootHeadOut = ProjectWorldToScreen(Predicted, Vector3(Localcam.y, Localcam.x, Localcam.z));
	}
	else
	{
		rootHeadOut = ProjectWorldToScreen(rootHead, Vector3(Localcam.y, Localcam.x, Localcam.z));
	}

	if (rootHeadOut.y != 0 || rootHeadOut.y != 0)
	{
		if ((GetCrossDistance(rootHeadOut.x, rootHeadOut.y, (width / 2), (height / 2)) <= aimfov * 8) || isaimbotting)
		{
			aimbot(rootHeadOut.x, rootHeadOut.y);
		}
	}
}

void aimbot(Vector3 Localcam)
{
	if (entityx != 0)
	{
		if (GetAimKey())
		{
			AimAt(entityx, Localcam);
		}
		else
		{
			isaimbotting = false;
		}
	}
}

void AIms(DWORD_PTR entity, Vector3 Localcam)
{
	float max = 100.f;

	uint64_t currentactormesh = read<uint64_t>(Driver_File, processID, entity + 0x278);

	Vector3 rootHead = GetBoneWithRotation(currentactormesh, 66);
	Vector3 rootHeadOut = ProjectWorldToScreen(rootHead, Vector3(Localcam.y, Localcam.x, Localcam.z));

	if (GetClosestPlayerToCrossHair(rootHeadOut, max, aimfov, entity))
		entityx = entity;
}

std::list<int> upper_part = { 65,66 };
std::list<int> right_arm = { 65, BONE_R_ARM_TOP, BONE_R_ARM_LOWER, BONE_MISC_R_HAND_1 };
std::list<int> left_arm = { 65, BONE_L_ARM_TOP, BONE_L_ARM_LOWER, BONE_MISC_L_HAND };
std::list<int> spine = { 65, BONE_PELVIS_1 };
std::list<int> lower_right = { BONE_PELVIS_2, BONE_R_THIGH ,76 };
std::list<int> lower_left = { BONE_PELVIS_2, BONE_L_THIGH ,69 };
std::list<std::list<int>> Skeleton = { upper_part, right_arm, left_arm, spine, lower_right, lower_left };

void DrawSkeleton(DWORD_PTR mesh)
{
	Vector3 neckpos = GetBoneWithRotation(mesh, 65);
	Vector3 pelvispos = GetBoneWithRotation(mesh, BONE_PELVIS_2);

	Vector3 previous(0, 0, 0);
	Vector3 current, p1, c1;
	Vector3 Localcam = Camera(Rootcomp);

	for (auto a : Skeleton)
	{
		previous = Vector3(0, 0, 0);
		for (int bone : a)
		{
			current = bone == 65 ? neckpos : (bone == BONE_PELVIS_2 ? pelvispos : GetBoneWithRotation(mesh, bone));
			if (previous.x == 0.f)
			{
				previous = current;
				continue;
			}

			p1 = ProjectWorldToScreen(previous, Vector3(Localcam.y, Localcam.x, Localcam.z));
			c1 = ProjectWorldToScreen(current, Vector3(Localcam.y, Localcam.x, Localcam.z));

			DrawLine(p1.x, p1.y, c1.x, c1.y, 2.f, 255.f, 0.f, 0.f, 200.f);

			previous = current;
		}
	}
}

void drawLoop(int width, int height) {
	menu();

	Uworld = read<DWORD_PTR>(Driver_File, processID, base_address + OFFSET_UWORLD);
	//printf(_xor_("Uworld: %p.\n").c_str(), Uworld);

	DWORD_PTR Gameinstance = read<DWORD_PTR>(Driver_File, processID, Uworld + 0x170);

	if (Gameinstance == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("Gameinstance: %p.\n").c_str(), Gameinstance);

	DWORD_PTR LocalPlayers = read<DWORD_PTR>(Driver_File, processID, Gameinstance + 0x38);

	if (LocalPlayers == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("LocalPlayers: %p.\n").c_str(), LocalPlayers);

	Localplayer = read<DWORD_PTR>(Driver_File, processID, LocalPlayers);

	if (Localplayer == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("LocalPlayer: %p.\n").c_str(), Localplayer);

	PlayerController = read<DWORD_PTR>(Driver_File, processID, Localplayer + 0x30);

	if (PlayerController == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("playercontroller: %p.\n").c_str(), PlayerController);

	LocalPawn = read<uint64_t>(Driver_File, processID, PlayerController + 0x298);

	if (LocalPawn == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("Pawn: %p.\n").c_str(), LocalPawn);

	Rootcomp = read<uint64_t>(Driver_File, processID, LocalPawn + 0x130);

	if (Rootcomp == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("Rootcomp: %p.\n").c_str(), Rootcomp);

	if (LocalPawn != 0) {
		localplayerID = read<int>(Driver_File, processID, LocalPawn + 0x18);
		//std::cout << _xor_("localplayerID = ").c_str() << localplayerID << std::endl;
	}

	Ulevel = read<DWORD_PTR>(Driver_File, processID, Uworld + 0x30);
	//printf(_xor_("Ulevel: %p.\n").c_str(), Ulevel);

	if (Ulevel == (DWORD_PTR)nullptr)
		return;

	DWORD ActorCount = read<DWORD>(Driver_File, processID, Ulevel + 0xA0);
	//printf(_xor_("ActorCount: %p.\n").c_str(), ActorCount);

	DWORD_PTR AActors = read<DWORD_PTR>(Driver_File, processID, Ulevel + 0x98);
	//printf(_xor_("AActors: %p.\n").c_str(), AActors);

	if (AActors == (DWORD_PTR)nullptr)
		return;

	for (int i = 0; i < ActorCount; i++)
	{
		uint64_t CurrentActor = read<uint64_t>(Driver_File, processID, AActors + i * 0x8);

		if (CurrentActor == (uint64_t)nullptr || CurrentActor == -1 || CurrentActor == NULL)
			continue;

		int curactorid = read<int>(Driver_File, processID, CurrentActor + 0x18);
		//std::cout << "current actor id = " << curactorid << std::endl;

		if (curactorid == 16868937)
		{
			uint64_t CurrentActorRootComponent = read<uint64_t>(Driver_File, processID, CurrentActor + 0x130);

			if (CurrentActorRootComponent == (uint64_t)nullptr || CurrentActorRootComponent == -1 || CurrentActorRootComponent == NULL)
				continue;

			Vector3 localactorpos = read<Vector3>(Driver_File, processID, Rootcomp + 0x11C);
			Vector3 actorpos = read<Vector3>(Driver_File, processID, CurrentActorRootComponent + 0x11C);
			Vector3 Localcam = Camera(Rootcomp);
			Vector3 actorposW2s = ProjectWorldToScreen(actorpos, Vector3(Localcam.y, Localcam.x, Localcam.z));

			float distance = localactorpos.Distance(actorpos) / 100.f;

			CHAR dist[50];
			sprintf_s(dist, _xor_("Mushroom [%.f m]").c_str(), distance);

			DrawString(dist, 10, actorposW2s.x, actorposW2s.y, 0, 1, 1);
		}
		else if (curactorid == 16819504)
		{
			uint64_t CurrentActorRootComponent = read<uint64_t>(Driver_File, processID, CurrentActor + 0x130);

			if (CurrentActorRootComponent == (uint64_t)nullptr || CurrentActorRootComponent == -1 || CurrentActorRootComponent == NULL)
				continue;

			Vector3 localactorpos = read<Vector3>(Driver_File, processID, Rootcomp + 0x11C);
			Vector3 actorpos = read<Vector3>(Driver_File, processID, CurrentActorRootComponent + 0x11C);
			Vector3 Localcam = Camera(Rootcomp);
			Vector3 actorposW2s = ProjectWorldToScreen(actorpos, Vector3(Localcam.y, Localcam.x, Localcam.z));

			float distance = localactorpos.Distance(actorpos) / 100.f;

			CHAR dist[50];
			sprintf_s(dist, _xor_("Boat [%.f m]").c_str(), distance);

			DrawString(dist, 10, actorposW2s.x, actorposW2s.y, 0, 1, 1);
		}
		else if (curactorid == localplayerID || curactorid == 16797703)
		{
			uint64_t CurrentActorRootComponent = read<uint64_t>(Driver_File, processID, CurrentActor + 0x130);

			if (CurrentActorRootComponent == (uint64_t)nullptr || CurrentActorRootComponent == -1 || CurrentActorRootComponent == NULL)
				continue;

			uint64_t currentactormesh = read<uint64_t>(Driver_File, processID, CurrentActor + 0x278);

			if (currentactormesh == (uint64_t)nullptr || currentactormesh == -1 || currentactormesh == NULL)
				continue;

			Vector3 Headpos = GetBoneWithRotation(currentactormesh, 66);
			Vector3 Localcam = Camera(Rootcomp);
			localactorpos = read<Vector3>(Driver_File, processID, Rootcomp + 0x11C);

			float distance = localactorpos.Distance(Headpos) / 100.f;

			if (distance < 1.5f)
				continue;

			//W2S
			Vector3 HeadposW2s = ProjectWorldToScreen(Headpos, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 bone0 = GetBoneWithRotation(currentactormesh, 0);
			Vector3 bottom = ProjectWorldToScreen(bone0, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Headbox = ProjectWorldToScreen(Vector3(Headpos.x, Headpos.y, Headpos.z + 15), Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Aimpos = ProjectWorldToScreen(Vector3(Headpos.x, Headpos.y, Headpos.z + 10), Vector3(Localcam.y, Localcam.x, Localcam.z));

			float Height1 = abs(Headbox.y - bottom.y);
			float Width1 = Height1 * 0.65;

			if (skeleton)
				DrawSkeleton(currentactormesh);

			if (BoxESP)
				DrawBox(Headbox.x - (Width1 / 2), Headbox.y, Width1, Height1, 0.f, 0.f, 0.f, 0.f, 0.5f, true);
			    DrawBox(Headbox.x - (Width1 / 2), Headbox.y, Width1, Height1, 2.f, 255.f, 0.f, 0.f, 200.f, false);

			if (EnemyESP)
				DrawString(_xor_ ("Nigger").c_str(), 13, Headbox.x, Headbox.y - 25, 0, 1, 1);

			if (DistanceESP)
			{
				CHAR dist[50];
				sprintf_s(dist, _xor_ ("[%.fm]").c_str(), distance);

				DrawString(dist, 13, Headbox.x + 40, Headbox.y - 25, 0, 1, 1);
			}

			if (LineESP)
				DrawLine(width / 2, height, bottom.x, bottom.y, 2.f, 255.f, 0.f, 0.f, 200.f);
			

			if (Aimbot)
			{
				AIms(CurrentActor, Localcam);
			}

			if (CircleFOV)
			{
				DrawCircle((float)(width / 2), (float)(height / 2), aimfov * 10.0f, 2.0f, 0.0f, 255.0f, 0.0f, 100.0f, false);
			}

			if (aimfov)
			{
				DrawString(_xor_("[U/D] FOVCircle >>").c_str(), 13, 10, 10 + 180, 255.f, 255.f, 255.f, 255.f);
				DrawString((std::to_string(aimfov)).c_str(), 13, 10 + 115, 10 + 180, 255.f, 255.f, 255.f, 255.f);
			}
		}
	}
	Sleep(2);
}

DWORD Aim(LPVOID in)
{
	while (1)
	{
		if (Aimbot)
		{
			Vector3 Localcam = Camera(Rootcomp);
			aimbot(Localcam);
		}
		Sleep(1);
	}
}

void main()
{
	SetConsoleTitle("Paradox");

	Driver_File = CreateFileW(_xor_(L"\\\\.\\FSALFSAKFKSL245215").c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (Driver_File == INVALID_HANDLE_VALUE)
	{
		printf(_xor_("[+] Load Driver first...\n").c_str());
		Sleep(2000);
		exit(0);
	}

	while (hwnd == NULL)
	{
		hwnd = FindWindowA(0, _xor_("Fortnite ").c_str());
		printf(_xor_("[!] Looking for Fortnite Process...\n").c_str());
		Sleep(1000);
	}
	GetWindowThreadProcessId(hwnd, &processID);

	RECT rect;
	if (GetWindowRect(hwnd, &rect))
	{
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;
	}

	info_t Input_Output_Data;
	Input_Output_Data.pid = processID;
	unsigned long int Readed_Bytes_Amount;

	DeviceIoControl(Driver_File, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
	base_address = (unsigned long long int)Input_Output_Data.data;

	std::printf(_xor_("[@] Process base address: %p.\n").c_str(), (void*)base_address);

	CreateThread(NULL, NULL, Menuthread, NULL, NULL, NULL);
	CreateThread(NULL, NULL, Aim, NULL, NULL, NULL);

	DirectOverlaySetOption(D2DOV_FONT_COURIER);
	DirectOverlaySetup(drawLoop, FindWindow(NULL, _xor_("Fortnite ").c_str()));

	getchar();
}
