#include "gtest/gtest.h"

#include <regex>

#include "yaml-cpp/yaml.h"

#include "cista/hash.h"

#include "boost/algorithm/string.hpp"

#include "openapi/gen_types.h"

using namespace openapi;

constexpr auto const example = R"(openapi: 3.0.3
info:
  title: MOTIS API
  description: This is the MOTIS routing API.
  contact:
    email: felix@triptix.tech
  license:
    name: MIT
    url: https://opensource.org/license/mit
  version: v1
externalDocs:
  description: Find out more about MOTIS
  url: http://triptix.tech
servers:
  - url: https://demo.triptix.tech
paths:
  /api/v1/plan:
    get:
      tags:
        - routing
      summary: Computes optimal connections from one place to another.
      parameters:
      - name: fromPlace
        in: query
        required: true
        description: latitude, longitude pair in degrees or stop id
        schema:
          type: string

      - name: toPlace
        in: query
        required: true
        description: latitude, longitude pair in degrees or stop id
        schema:
          type: string

      - name: date
        in: query
        required: false
        description: |
          Optional. Defaults to the current date.

          Departure date ($arriveBy=false) / arrival date ($arriveBy=true), format: 06-28-2024
        schema:
          type: string

      - name: time
        in: query
        required: false
        description: |
          Optional. Defaults to the current time.

          Meaning depending on `arriveBy`
            - Departure time for `arriveBy=false`
            - Arrival time for `arriveBy=true`

          Format:
            - 12h format: 7:06pm
            - 24h format: 19:06
        schema:
          type: string

      - name: maxTransfers
        in: query
        required: false
        description: |
          The maximum number of allowed transfers.
          If not provided, the routing uses the server-side default value
          which is hardcoded and very high to cover all use cases.

          *Warning*: Use with care. Setting this too low can lead to
          optimal (e.g. the fastest) journeys not being found.
          If this value is too low to reach the destination at all,
          it can lead to slow routing performance.
        schema:
          type: integer

      - name: maxHours
        in: query
        required: false
        description: |
          The maximum travel time in hours.
          If not provided, the routing to uses the value
          hardcoded in the server which is usually quite high.

          *Warning*: Use with care. Setting this too low can lead to
          optimal (e.g. the least transfers) journeys not being found.
          If this value is too low to reach the destination at all,
          it can lead to slow routing performance.
        schema:
          type: number

      - name: minTransferTime
        in: query
        required: false
        description: Minimum transfer time for each transfer.
        schema:
          type: integer

      - name: transferTimeFactor
        in: query
        required: false
        description: Factor to multiply transfer times with.
        schema:
          type: integer
          default: 1

      - name: mode
        in: query
        required: false
        description: |
          A comma separated list of modes.

          Default if not provided: `WALK,TRANSIT`

          # Street modes

            - `WALK` Walking some or all of the way of the route.
            - `BIKE`: Cycling for the entirety of the route or taking a bicycle onto the public transport and cycling from the arrival station to the destination.
            - `BIKE_RENTAL`: Taking a rented, shared-mobility bike for part or the entirety of the route.
            - `BIKE_TO_PARK`: Leaving the bicycle at the departure station and walking from the arrival station to the destination. This mode needs to be combined with at least one transit mode otherwise it behaves like an ordinary bicycle journey.
            - `CAR`: Driving your own car the entirety of the route. This can be combined with transit, where will return routes with a Kiss & Ride component. This means that the car is not parked in a permanent parking area but rather the passenger is dropped off (for example, at an airport) and the driver continues driving the car away from the drop off location.
            - `CAR_PARK` | `CAR_TO_PARK`: Driving a car to the park-and-ride facilities near a station and taking publictransport. This mode needs to be combined with at least one transit mode otherwise, it behaves like an ordinary car journey.
            - `CAR_HAILING`: Using a car hailing app like Uber or Lyft to get to a train station or all the way to the destination.
            - `CAR_PICKUP`: Walking to a pickup point along the road, driving to a drop-off point along the road, and walking the rest of the way. This can include various taxi-services or kiss & ride.
            - `CAR_RENTAL`: Walk to a car rental point, drive to a car rental drop-off point and walk the rest of the way. This can include car rental at fixed locations or free-floating services.
            - `FLEXIBLE`: Encompasses all types of on-demand and flexible transportation for example GTFS Flex or NeTEx Flexible Stop Places.
            - `SCOOTER_RENTAL`: Walking to a scooter rental point, riding a scooter to a scooter rental drop-off point, and walking the rest of the way. This can include scooter rental at fixed locations or free-floating services.

          # Transit modes

            - `TRANSIT`: translates to `RAIL,SUBWAY,TRAM,BUS,FERRY,AIRPLANE,COACH`
            - `TRAM`: trams
            - `SUBWAY`: subway trains
            - `FERRY`: ferries
            - `AIRPLANE`: airline flights
            - `BUS`: short distance buses (does not include `COACH`)
            - `COACH`: long distance buses (does not include `BUS`)
            - `RAIL`: translates to `HIGHSPEED_RAIL,LONG_DISTANCE_RAIL,NIGHT_RAIL,REGIONAL_RAIL,REGIONAL_FAST_RAIL`
            - `HIGHSPEED_RAIL`: long distance high speed trains (e.g. TGV)
            - `LONG_DISTANCE`: long distance inter city trains
            - `NIGHT_RAIL`: long distance night trains
            - `COACH`: long distance buses
            - `REGIONAL_FAST_RAIL`: regional express routes that skip low traffic stops to be faster
            - `REGIONAL_RAIL`: regional train

        schema:
          type: array
          items:
            type: string
            minItems: 1
            enum:
              # === Street ===
              - WALK
              - BIKE
              - CAR
              - BIKE_RENTAL
              - BIKE_TO_PARK
              - CAR_PARK
              - CAR_TO_PARK
              - CAR_HAILING
              - CAR_SHARING
              - CAR_PICKUP
              - CAR_RENTAL
              - FLEXIBLE
              - SCOOTER_RENTAL
              # === Transit ===
              - TRANSIT
              - TRAM
              - SUBWAY
              - FERRY
              - AIRPLANE
              - BUS
              - COACH
              - RAIL
              - HIGHSPEED_RAIL
              - LONG_DISTANCE
              - NIGHT_RAIL
              - REGIONAL_FAST_RAIL
              - REGIONAL_RAIL
        explode: false

      - name: numItineraries
        in: query
        required: false
        description: |
          The minimum number of itineraries to compute.
          This is only relevant if `timetableView=true`.
          The default value is 5.
        schema:
          type: integer
          default: 5

      - name: pageCursor
        in: query
        required: false
        description: |
          Use the cursor to go to the next "page" of itineraries.
          Copy the cursor from the last response and keep the original request as is.
          This will enable you to search for itineraries in the next or previous time-window.
        schema:
          type: string

      - name: timetableView
        in: query
        required: false
        description: |
          Optional. Default is `true`.

          Search for the best trip options within a time window.
          If true two itineraries are considered optimal
          if one is better on arrival time (earliest wins)
          and the other is better on departure time (latest wins).
          In combination with arriveBy this parameter cover the following use cases:

          `timetable=false` = waiting for the first transit departure/arrival is considered travel time:
            - `arriveBy=true`: event (e.g. a meeting) starts at 10:00 am,
              compute the best journeys that arrive by that time (maximizes departure time)
            - `arriveBy=false`: event (e.g. a meeting) ends at 11:00 am,
              compute the best journeys that depart after that time

          `timetable=true` = optimize "later departure" + "earlier arrival" and give all options over a time window:
            - `arriveBy=true`: the time window around `date` and `time` refers to the arrival time window
            - `arriveBy=false`: the time window around `date` and `time` refers to the departure time window
        schema:
          type: boolean
          default: false

      - name: searchWindow
        in: query
        required: false
        description: |
          Optional. Default is 2 hours which is `7200`.

          The length of the search-window in seconds. Default value two hours.

            - `arriveBy=true`: number of seconds between the earliest departure time and latest departure time
            - `arriveBy=false`: number of seconds between the earliest arrival time and the latest arrival time
        schema:
          type: integer
          default: 7200
          minimum: 0

      - name: maxPreTransitTime
        in: query
        required: false
        description: |
          Optional. Default is 15min which is `900`.
          Maximum time in seconds for the first street leg.
        schema:
          type: integer
          default: 900
          minimum: 0

      - name: maxPostTransitTime
        in: query
        required: false
        description: |
          Optional. Default is 15min which is `900`.
          Maximum time in seconds for the last street leg.
        schema:
          type: integer
          default: 900
          minimum: 0

      responses:
        '200':
          description: routing result
          content:
            application/json:
              schema:
                type: object
                properties:
                  requestParameters:
                    description: "the routing query"
                    type: object
                    additionalProperties:
                      type: string
                  debugOutput:
                    description: "debug statistics"
                    type: object
                    additionalProperties:
                      type: string
                  date:
                    description: "The time and date of travel"
                    type: integer
                  from:
                    $ref: '#/components/schemas/Place'
                  to:
                    $ref: '#/components/schemas/Place'
                  itineraries:
                    description: list of itineraries
                    type: array
                    items:
                      $ref: '#/components/schemas/Itinerary'

        '400':
          description: Invalid routing parameters
components:
  schemas:
    Place:
      type: object
      properties:
        name:
          description: name of the transit stop / PoI / address
          type: string
        lat:
          description: latitude
          type: number
        lon:
          description: longitude
          type: number
        arrival:
          description: arrival time, format = unixtime in milliseconds
          type: integer
        departure:
          description: departure time, format = unixtime in milliseconds
          type: integer

    RelativeDirection:
      type: string
      enum:
      - DEPART
      - HARD_LEFT
      - LEFT
      - SLIGHTLY_LEFT
      - CONTINUE
      - SLIGHTLY_RIGHT
      - RIGHT
      - HARD_RIGHT
      - CIRCLE_CLOCKWISE
      - CIRCLE_COUNTERCLOCKWISE
      - ELEVATOR
      - UTURN_LEFT
      - UTURN_RIGHT

    AbsoluteDirection:
      type: string
      enum:
      - NORTH
      - NORTHEAST
      - EAST
      - SOUTHEAST
      - SOUTH
      - SOUTHWEST
      - WEST
      - NORTHWEST

    StepInstruction:
      type: object
      properties:
        relativeDirection:
          $ref: '#/components/schemas/RelativeDirection'
        absoluteDirection:
          $ref: '#/components/schemas/AbsoluteDirection'
        distance:
          description: The distance in meters that this step takes.
          type: number
        streetName:
          description: The name of the street.
          type: string
        exit:
          description: When exiting a highway or traffic circle, the exit name/number.
          type: string
        stayOn:
          description: |
            Indicates whether or not a street changes direction at an intersection.
          type: boolean
        area:
          description: |
            This step is on an open area, such as a plaza or train platform,
            and thus the directions should say something like "cross"
          type: boolean
        lon:
          description: The longitude of start of the step
          type: number
        lat:
          description: The latitude of start of the step
          type: number

    VertexType:
      type: string
      description: |
        - `NORMAL` - latitude / longitude coordinate or address
        - `BIKESHARE` - bike sharing station
        - `BIKEPARK` - bike parking
        - `TRANSIT` - transit stop
      enum:
      - NORMAL
      - BIKESHARE
      - BIKEPARK
      - TRANSIT

    FeedScopedId:
      title: FeedScopedId
      type: object
      properties:
        feedId:
          type: string
        id:
          type: string

    EncodedPolyline:
      title: EncodedPolylineBean
      type: object
      properties:
        points:
          description: The encoded points of the polyline.
          type: string
        length:
          description: The number of points in the string
          type: integer

    Itinerary:
      type: object
      properties:
        duration:
          description: journey duration in seconds
          type: integer
        startTime:
          type: integer
          description: journey departure time, format = unixtime in milliseconds
        endTime:
          type: integer
          description: journey arrival time, format = unixtime in milliseconds
        walkTime:
          type: integer
          description: How much time is spent walking, in seconds.
        transitTime:
          type: integer
          description: How much time is spent on transit, in seconds.
        waitingTime:
          type: integer
          description: How much time is spent waiting for transit to arrive, in seconds.
        walkDistance:
          type: integer
          description: How far the user has to walk, in meters.
        transfers:
          type: integer
          description: The number of transfers this trip has.
        legs:
          description: Journey legs
          type: array
          items:
            $ref: '#/components/schemas/Leg'

    Leg:
      type: object
      properties:
        startTime:
          type: integer
          description: leg departure time, format = unixtime in milliseconds
        endTime:
          type: integer
          description: leg arrival time, format = unixtime in milliseconds
        departureDelay:
          type: integer
          description: |
            The offset from the scheduled departure time of the boarding stop in this leg.
            Scheduled time of departure at boarding stop = startTime - departureDelay
        arrivalDelay:
          type: integer
          description: |
            The offset from the scheduled arrival time of the boarding stop in this leg.
            Scheduled time of arrival at boarding stop = endTime - arrivalDelay
        realTime:
          description: Whether there is real-time data about this leg
          type: boolean
        distance:
          description: The distance traveled while traversing this leg in meters.
          type: number
        interlineWithPreviousLeg:
          description: For transit legs, if the rider should stay on the vehicle as it changes route names.
          type: boolean
        route:
          description: |
            For transit legs, the route of the bus or train being used.
            For non-transit legs, the name of the street being traversed.
          type: string
        headsign:
          description: |
            For transit legs, the headsign of the bus or train being used.
            For non-transit legs, null
          type: string
        agencyName:
          type: string
        agencyUrl:
          type: string
        routeColor:
          type: string
        routeTextColor:
          type: string
        routeType:
          type: string
        routeId:
          type: string
        agencyId:
          type: string
        tripId:
          type: string
        serviceDate:
          type: string
        duration:
          description: Leg duration in seconds
          type: integer
        from:
          $ref: '#/components/schemas/Place'
        to:
          $ref: '#/components/schemas/Place'
        legGeometry:
          $ref: '#/components/schemas/EncodedPolyline'
        steps:
          description: |
            A series of turn by turn instructions
            used for walking, biking and driving.
          type: array
          items:
            $ref: '#/components/schemas/StepInstruction'
        mode:
          description: Transport mode for this leg
          type: string
          enum:
            - WALK
            - BIKE
            - CAR
            - TRANSIT
            - TRAM
            - SUBWAY
            - FERRY
            - AIRPLANE
            - BUS
            - COACH
            - RAIL
)";

using value = std::variant<int, double, bool, std::string>;

struct schema {
  std::vector<value> enum_;
  value default_;
  value min_;
  value max_;
  value multiple_of_;
  bool unique_items_;
};

struct array {
  schema items_;
};

constexpr auto const kX = R"(
- name: mode
  in: query
  required: false
  schema:
    type: array
    items:
      type: string
      minItems: 1
      enum:
        - WALK
        - TRANSIT
  explode: false
)";

constexpr auto const kEnum = R"(
paths:
  /items:
    get:
      parameters:
        - in: query
          name: sort
          description: Sort order
          schema:
            type: string
            enum: [asc, desc]
)";

TEST(abc, def) {
  static auto const path_param_regex = std::regex{R"(\{([^\}]*)\})"};

  auto const config = YAML::Load(example);
  auto out = std::stringstream{};

  out << "struct service {\n";
  out << "}\n\n";

  out << "template <typename WebServer, typename Executor>\n"
      << "void register(WebServer& app, Executor& exec, service& s) {\n";
  for (auto const& e : config["paths"]) {
    auto const path = e.first.as<std::string>();
    auto const path_replaced =
        std::regex_replace(path, path_param_regex, ":$1");

    for (auto const& x : e.second) {
      auto const path_params = get_path_params(x.second);
      auto const id = x.second["operationId"].as<std::string>();
      auto const& method =
          boost::algorithm::to_lower_copy(x.first.as<std::string>());

      out << "  app." << method << "(\"" << path_replaced
          << "\", [&](auto *res, auto *req) {\n"
          << "    s." << id << "(";

      auto first = true;
      auto const separator = [&]() {
        if (first) {
          out << "\n      ";
          first = false;
        } else {
          out << ",\n      ";
        }
      };
      auto it = path.cbegin();
      auto match = std::smatch{};
      while (regex_search(it, path.cend(), match, path_param_regex)) {
        separator();
        auto const param = match[1];
        out << "utl::parse<" << type_to_str(path_params.at(param))
            << ">(req->getParameter(\"" << match[1] << "\"))";
        it = match.suffix().first;
      }

      for (auto const& m : match) {
        separator();
      }

      out << "\n    );\n"
          << "  });\n";
    }
  }
  out << "}\n";

  std::cout << out.str() << "\n";

  auto ss = std::stringstream{};
  generate_types(ss, "YEAH");
  EXPECT_EQ("Hello!\n", ss.str());
}