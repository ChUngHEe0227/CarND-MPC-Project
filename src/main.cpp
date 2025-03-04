#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <cassert> 
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "helpers.h"
#include "json.hpp"
#include "MPC.h"

// for convenience
using nlohmann::json;
using std::string;
using std::vector;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }


void glob_to_local(Eigen::VectorXd x_glob, Eigen::VectorXd y_glob, 
                   double psi, double px, double py, 
                   Eigen::VectorXd &x_local, Eigen::VectorXd &y_local){
  
  double cc = cos(psi);
  double ss = sin(psi);
  double x_local_, y_local_;
  double x_diff_glob, y_diff_glob;

  for(int i=0; i<x_glob.size(); i++){
    x_diff_glob = x_glob[i] - px;
    y_diff_glob = y_glob[i] - py;

    x_local_ = x_diff_glob * cc + y_diff_glob * ss;
    y_local_ = - x_diff_glob * ss + y_diff_glob * cc;

    x_local[i] = x_local_;
    y_local[i] = y_local_;
  }


}
void polyeval_vec(Eigen::VectorXd ptsx, Eigen::VectorXd coeffs, 
                  vector<double> &ptsx_poly, vector<double> &ptsy_poly){
  double px, py;
  for(int i=0; i<ptsx.size(); i++){
    px = ptsx[i];
    ptsx_poly[i] = px; // x

    py = polyeval(coeffs, px);
    ptsy_poly[i] = py; // y
  }
}


int main() {
  uWS::Hub h;

  // MPC is initialized here!
  MPC mpc;

  h.onMessage([&mpc](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    string sdata = string(data).substr(0, length);
    std::cout << sdata << std::endl;
    if (sdata.size() > 2 && sdata[0] == '4' && sdata[1] == '2') {
      string s = hasData(sdata);
      if (s != "") {
        auto j = json::parse(s);
        string event = j[0].get<string>();
        if (event == "telemetry") {
          // j[1] is the data JSON object
          vector<double> ptsx = j[1]["ptsx"];
          vector<double> ptsy = j[1]["ptsy"];
          double px = j[1]["x"];
          double py = j[1]["y"];
          double psi = j[1]["psi"];
          double v = j[1]["speed"];
          double steering_angle = j[1]["steering_angle"];
          double throttle = j[1]["throttle"];

          double throttle_real;
          if (throttle > 0){
            throttle_real = throttle * 5;
          }else{
            throttle_real = throttle * 12;
          }
          double* ptsx_ptr = &ptsx[0];
          Eigen::Map<Eigen::VectorXd> ptsx_(ptsx_ptr, ptsx.size());
          
          double* ptsy_ptr = &ptsy[0];
          Eigen::Map<Eigen::VectorXd> ptsy_(ptsy_ptr, ptsy.size());
          
          Eigen::VectorXd ptsx_local(ptsx.size());
          Eigen::VectorXd ptsy_local(ptsy.size()); 
          glob_to_local(ptsx_, ptsy_, psi, px, py, ptsx_local, ptsy_local);
          /**
           * TODO: Calculate steering angle and throttle using MPC.
           * Both are in between [-1, 1].
           */
          Eigen::VectorXd coeffs = polyfit(ptsx_local, ptsy_local, 3);
          double d_ahead = v * 0.15; 
          double cte = polyeval(coeffs, d_ahead);
          double epsi = - atan(coeffs[1]);
          
        
          Eigen::VectorXd state(6+2);
          state << d_ahead, 0, 0, v, cte, epsi, -steering_angle, throttle_real;


          // solve MPC!
          auto vars = mpc.Solve(state, coeffs);

          double steer_value;
          double throttle_value;

          steer_value = - vars[0] / 2.67; // normalize to [-1, 1]
          if(vars[1] >= 0){
            throttle_value = vars[1] / 5;
          }else{
            throttle_value = vars[1] / 12;
          }
          json msgJson;
          // NOTE: Remember to divide by deg2rad(25) before you send the 
          //   steering value back. Otherwise the values will be in between 
          //   [-deg2rad(25), deg2rad(25] instead of [-1, 1].
          msgJson["steering_angle"] = steer_value;
          msgJson["throttle"] = throttle_value;

          // Display the MPC predicted trajectory 
          vector<double> mpc_x_vals;
          vector<double> mpc_y_vals;
          mpc_x_vals = mpc.pred_x;
          mpc_y_vals = mpc.pred_y;
          /**
           * TODO: add (x,y) points to list here, points are in reference to 
           *   the vehicle's coordinate system the points in the simulator are 
           *   connected by a Green line
           */

          msgJson["mpc_x"] = mpc_x_vals;
          msgJson["mpc_y"] = mpc_y_vals;
          vector<double> next_x_vals (ptsx.size());
          vector<double> next_y_vals (ptsy.size());
        
          polyeval_vec(ptsx_local, coeffs, next_x_vals, next_y_vals);

          // Display the waypoints/reference line

          /**
           * TODO: add (x,y) points to list here, points are in reference to 
           *   the vehicle's coordinate system the points in the simulator are 
           *   connected by a Yellow line
           */

          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;

          auto msg = "42[\"steer\"," + msgJson.dump() + "]";
          std::cout << throttle_value << std::endl;
          // Latency
          // The purpose is to mimic real driving conditions where
          //   the car does actuate the commands instantly.
          //
          // Feel free to play around with this value but should be to drive
          //   around the track with 100ms latency.
          //
          // NOTE: REMEMBER TO SET THIS TO 100 MILLISECONDS BEFORE SUBMITTING.
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }  // end "telemetry" if
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }  // end websocket if
  }); // end h.onMessage

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  
  h.run();
}
