    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <string.h>
    #include <signal.h>
    #include <ctype.h>
    #include <sys/wait.h>
    #include <stdbool.h>
    #include <time.h>
    #include <math.h>
    #define CAPACITY_X 300
    #define CAPACITY_Y 400
    #define CAPACITY_Z 500
    #define MAXIMUMORDER 10000

    typedef struct {
        char orderNo[20];
        char dueDate[20];
        int qty;
        char productName[20];
    } Order;

    typedef struct {
        int capacity;
        char plantName[4];
        int capacityUsed;
        int capacityWast;
        // char orderNo[20];
    } Plant;//MAXIMUMORDER

    typedef struct {
        Plant plant;
        Order Order;
        int dayUsed;
        int reject;
        char currentDate[20]; 
        char productStartDate[20]; 
        char periodStart[20];
        char periodEnd[20];
    } Order_Plant;  //muti-table database to store data

    int orderPlantCounter;  //orderPlantCounter is the order_plant list index
    Order_Plant orderPlant[MAXIMUMORDER];
    Order_Plant rejectedOrderPlant[MAXIMUMORDER];

    pid_t Pids[3];

    int pToC1[2], c1ToC2[2], c2ToC3[2], c3ToP[2];
    int pid, i, n, j; 
    char buf_p[80], buf_c1[80], buf_c2[80], buf_c3[80];

    char* tokens[20];
    int tokenCount = 0;

    char algoType[20];
    char reportName[20];

    // for add period
    int period;
    char start_date[20];
    char end_date[20];
    char plantDays[3][20];//current date


    // for add order
    int orderCounter = 0;
    Order orderList[MAXIMUMORDER];

    //plant day workload counter : X Y Z
    int counter[3] = {0};
    int storeBest[3] = {0};
    int storeFit[3] = {0};
    int wastCounter[3] = {0};
    int previous_Max;

    //3 plant 
    // Plant plantX;Plant plantY; Plant plantZ;
    Plant plant[3];
    int plantCapacity[3] = {CAPACITY_X,CAPACITY_Y,CAPACITY_Z};
    char plantName[3][1] = {'X','Y','Z'};

    //count day is for process can go ahead or not
    //int countDay = 0;
    char countDay[11]; // Format YYYY-MM-DD

    // ======================================== Function ========================================
    //set default data
    void init(){
        int i;
        for(i = 0; i<3;i++){
            plant[i].capacity = plantCapacity[i];
            strcpy(&plant[i].plantName[0], plantName[i]);
            plant[i].plantName[1] = '\0';
            //strcpy(&plant[i].currentDate, '');
        }

        strcpy(start_date, "");
        strcpy(end_date, "");
    }
    int min_difference;

    //find each ase the max number of the working days
    int findMax(int* numbers, int size) {
        int max = 0;
        for ( i = 0; i < size; i++) {
            if(max<numbers[i]){
                max = numbers[i];
            }
        }
        return max;
    }

    //round up to 100
    int roundUp(int number) {
        int remainder = number % 100;  // Calculate the remainder when divided by 100
        
        if (remainder == 0) {
            // The number is already a multiple of 100
            return number;
        } else {
            // Round up to the nearest multiple of 100
            return number + (100 - remainder);
        }
    }

    void findCombinations(int target) { //orderPlantCounter is the order_plant list index
        //printf("target : %d, index:%d\n",target, index);
        //reset counter 
        for(i = 0; i<3; i++){
            counter[i] = 0;
        }
        int x, y, z;
        for(x = 0; x <= target/300; x++) {
            for(y = 0; y <= target/400; y++) {
                for(z = 0; z <= target/500; z++) {
                    if(300*x + 400*y + 500*z == target) {
                        //find the max no. of day of 3 plant
                        counter[0] = x;
                        counter[1] = y;
                        counter[2] = z;
                        int max = findMax(counter,sizeof(counter) / sizeof(counter[0]));
                        //to store the case if their max number is loser than previous
                        if (max<previous_Max){
                            previous_Max = max;
                            for (i = 0; i < sizeof(counter) / sizeof(counter[0]); i++) {//i is the index of plant
                                storeBest[i] = counter[i];
                            }
                        }  
                    }
                }
            }
        }
        return;
    }

    void clearTokens(char* tokens[], int size) {
        for (i = 0; i < size; i++) {
            if (tokens[i] != NULL) {
                free(tokens[i]);  
                tokens[i] = NULL; 
            }
        }
    }

    int calDays(char start[20], char end[20]) {
        struct tm tmStart, tmEnd;
        time_t start_time, end_time;
        double seconds;
        memset(&tmStart, 0, sizeof(struct tm));
        memset(&tmEnd, 0, sizeof(struct tm));
        strptime(start, "%Y-%m-%d", &tmStart);
        strptime(end, "%Y-%m-%d", &tmEnd);
        start_time = mktime(&tmStart);
        end_time = mktime(&tmEnd);
        if (start_time == (time_t) -1 || end_time == (time_t) -1) {
            printf("Error converting time.\n");
            return -1;
        }
        seconds = difftime(end_time, start_time);
        return (int)(seconds / (60 * 60 * 24)) + 1;
    }

    //maybe need to -1 days 06-01 use 3 days will become 06-04 but 01days are not used
    char* addMoreDays(const char* dateString, int daysToAdd) {
        static char newDate[11]; 
        struct tm tm;
        memset(&tm, 0, sizeof(struct tm));
        strptime(dateString, "%Y-%m-%d", &tm);
        time_t t = mktime(&tm);
        if (t == -1) {
            fprintf(stderr, "Could not convert time.\n");
            return NULL;
        }
        t += daysToAdd * 86400;
        struct tm *newTm = localtime(&t);
        if (newTm == NULL) {
            fprintf(stderr, "Could not compute new date.\n");
            return NULL;
        }
        strftime(newDate, sizeof(newDate), "%Y-%m-%d", newTm);
        return newDate;
    }

    bool compare2Date(const char* date1, const char* date2) {
        struct tm tm1, tm2;
        time_t t1, t2;
        memset(&tm1, 0, sizeof(struct tm));
        memset(&tm2, 0, sizeof(struct tm));
        strptime(date1, "%Y-%m-%d", &tm1);
        t1 = mktime(&tm1);
        strptime(date2, "%Y-%m-%d", &tm2);
        t2 = mktime(&tm2);
        return difftime(t1, t2) < 0;
    }

    void addPERIOD(char start_date[20], char end_date[20], char* tokens[]){
        strcpy(start_date, tokens[1]);
        strcpy(end_date, tokens[2]);
    }

    Order addORDER(char* tokens[]){
        Order order;

            strcpy(order.orderNo, tokens[1]);
            strcpy(order.dueDate, tokens[2]);
            int num = atoi(tokens[3]);
            order.qty = num;
            strcpy(order.productName, tokens[4]);
        return order;
    }

    int addBATCH(char *filename, Order orderList[MAXIMUMORDER], int orderCounter, char end_date[20]) {
        Order order;
        FILE *file = fopen(filename, "r");
        if (!file) {
            printf("Failed to open file: %s\n", filename);
            return 0;
        }

        char line[256];
        while (fgets(line, sizeof(line), file)) {
            char cmd[20], orderNo[20], dueDate[11], productName[20];
            int qty;
            if (sscanf(line, "%s %s %s %d %s", cmd, orderNo, dueDate, &qty, productName) == 5 && compare2Date(dueDate, addMoreDays(end_date,1))&& compare2Date(addMoreDays(start_date,-1),dueDate)) {
                
                strcpy(order.orderNo, orderNo);
                strcpy(order.dueDate, dueDate);
                order.qty = qty;
                strcpy(order.productName, productName);
                orderList[orderCounter] = order;
                // printf("OrderCounter: %d, Order No: %s, Due Date: %s, Quantity: %d, Product Name: %s\n",
                //        orderCounter, order.orderNo, order.dueDate, order.qty, order.productName);
                orderCounter++;
            } else {
                printf("Failed to parse line: %s", line);
            }
        }
        fclose(file);
        return orderCounter;
    }

    int rejectCounter = 0;
    //countDay is share variable for different plant
    //algo method must be in the loop, and based on objectlist
    void algo(int n){ //n  is the orderList index
        //debug printf("Order qty:%d\n", orderList[n].qty);
        int roundUped = roundUp(orderList[n].qty);
        findCombinations(roundUped);
        int wastCounted = 0;
        for(i=0;i<3;i++){
            wastCounter[i] = 0;
        }
        int no, no2, no3;
        //printf("Combination: ");//debug
        //loop order list
        for (i = 0; i < sizeof(counter) / sizeof(counter[0]);) {
            //printf("%d ", storeBest[i]);//debug
            //reject
            no = calDays(addMoreDays(plantDays[i],storeBest[i]),orderList[n].dueDate);
            no2 = calDays(addMoreDays(plantDays[i+1],storeBest[i+1]),orderList[n].dueDate);
            no3 = calDays(addMoreDays(plantDays[i+2],storeBest[i+2]),orderList[n].dueDate);
            //find fit case
            int checker1,checker2,checker3;
            int x, y, z,checked = 0;
            for(x = 0; x <= roundUped/300; x++) {
                for(y = 0; y <= roundUped/400; y++) {
                    for(z = 0; z <= roundUped/500; z++) {
                        if(300*x + 400*y + 500*z == roundUped) {
                             checker1 = calDays(addMoreDays(plantDays[0],x),orderList[n].dueDate);
                             checker2 = calDays(addMoreDays(plantDays[1],y),orderList[n].dueDate);
                             checker3 = calDays(addMoreDays(plantDays[2],z),orderList[n].dueDate);
                            if(checker1>0&&checker2>0&&checker3>0){
                                storeFit[0] = x;
                                storeFit[1] = y;
                                storeFit[2] = z;
                                checked = 1;
                            }
                             checker1 = calDays(addMoreDays(plantDays[0],2),orderList[n].dueDate);
                             checker2 = calDays(addMoreDays(plantDays[1],1),orderList[n].dueDate);
                             checker3 = calDays(addMoreDays(plantDays[0],1),orderList[n].dueDate);
                            if(roundUped==600&&checker1<=0&&checker2>0&&checker3>0){
                                storeFit[0] = 1;
                                storeFit[1] = 1;
                                storeFit[2] = 0;
                                checked = 1;
                            }
                        }
                    }
                }
            }
            //if(orderList[i].dueDate)
            //printf("%d\n", no);
            if(no>0&&no2>0&&no3>0){
                int plantCounter;
                for(plantCounter = 0; plantCounter<3;plantCounter++){

                    orderPlant[orderPlantCounter].plant = plant[i];
                    orderPlant[orderPlantCounter].Order = orderList[n];
                    orderPlant[orderPlantCounter].dayUsed = storeBest[i];
                    //if there have wast use, and the plant are used, and the wast have not count
                    if((roundUped-orderList[n].qty)>0 && storeBest[i]!=0 && wastCounted == 0){
                        orderPlant[orderPlantCounter].plant.capacityWast = (roundUped-orderList[n].qty);
                        wastCounted =1;
                        orderPlant[orderPlantCounter].plant.capacityUsed = plant[plantCounter].capacity-(roundUped-orderList[n].qty);
                    }else{
                        orderPlant[orderPlantCounter].plant.capacityUsed = plant[plantCounter].capacity;
                    }
                    strcpy(orderPlant[orderPlantCounter].productStartDate, plantDays[i]);
                    strcpy(plantDays[i],addMoreDays(plantDays[i],storeBest[i]));
                    //printf("Days used: %s %d %d\n", plantDays[i], storeBest[i],i);
                    strcpy(orderPlant[orderPlantCounter].currentDate, plantDays[i]);
                    //store period start and end into order_plant ##
                    strcpy(orderPlant[orderPlantCounter].periodStart, start_date);
                    strcpy(orderPlant[orderPlantCounter].periodEnd, end_date);
                    
                    orderPlantCounter++;
                    i++;
                }
            }else if(checked == 1){//if the best case are not useable than use fit case
                int plantCounter;
                for(plantCounter = 0; plantCounter<3;plantCounter++){

                    orderPlant[orderPlantCounter].plant = plant[i];
                    orderPlant[orderPlantCounter].Order = orderList[n];
                    orderPlant[orderPlantCounter].dayUsed = storeFit[i];
                    //if there have wast use, and the plant are used, and the wast have not count
                    if((roundUped-orderList[n].qty)>0 && storeFit[i]!=0 && wastCounted == 0){
                        orderPlant[orderPlantCounter].plant.capacityWast = (roundUped-orderList[n].qty);
                        wastCounted =1;
                        orderPlant[orderPlantCounter].plant.capacityUsed = plant[plantCounter].capacity-(roundUped-orderList[n].qty);
                    }else{
                        orderPlant[orderPlantCounter].plant.capacityUsed = plant[plantCounter].capacity;
                    }
                    strcpy(orderPlant[orderPlantCounter].productStartDate, plantDays[i]);
                    strcpy(plantDays[i],addMoreDays(plantDays[i],storeFit[i]));
                    //printf("Days used: %s %d %d\n", plantDays[i], storeFit[i],i);
                    strcpy(orderPlant[orderPlantCounter].currentDate, plantDays[i]);
                    //store period start and end into order_plant ##
                    strcpy(orderPlant[orderPlantCounter].periodStart, start_date);
                    strcpy(orderPlant[orderPlantCounter].periodEnd, end_date);
                    orderPlantCounter++;
                    i++;
                }
            }else{
                //count reject number
                int plantCounter;
                for(plantCounter = 0; plantCounter<3;plantCounter++){
                    rejectedOrderPlant[rejectCounter].dayUsed = storeBest[i];
                    rejectedOrderPlant[rejectCounter].plant = plant[i];
                    rejectedOrderPlant[rejectCounter].Order = orderList[n];
                    rejectCounter++;
                    i++;
                }
            }
        }        
        //printf("\n");//debug
    }

    void p2C1CreateFile(Order orderList[], int orderCounter, char algoType[10], char start_date[20], char end_date[20], char reportName[20]){
        // Open the file for writing
        FILE *file = fopen("forSchedulingModule.txt", "w");
        if (file == NULL) {
            printf("Error opening file");
            return;
        }
        fprintf(file, "%s %s\n", algoType, reportName);
        fprintf(file, "%s %s\n", start_date, end_date);
        for ( i = 0; i < orderCounter; ++i) {
            Order order = orderList[i];
            fprintf(file, "%s %s %d %s\n",
                    order.orderNo, order.dueDate, order.qty, order.productName);
        }
        fclose(file);
    }

    int c1ReadFile(char *filename, Order orderList[MAXIMUMORDER], char start_date[20], char end_date[20], char algoType[20], char reportName[20]) {
        Order order;
        int orderCounter = 0;
        FILE *file = fopen(filename, "r");
        if (!file) {
            printf("Failed to open file: %s\n", filename);
            return 0;
        }
        char line[256];
        int lineNo = 0;
        while (fgets(line, sizeof(line), file)) {
            char cmd[20], orderNo[20], dueDate[11], productName[20];    //algoType[10], 
            int qty;
            if(lineNo == 0){
                sscanf(line, "%s %s", algoType, reportName);
            }else if(lineNo == 1){
                sscanf(line, "%s %s", start_date, end_date);
            }else{
                if (sscanf(line, "%s %s %d %s", orderNo, dueDate, &qty, productName) == 4) {
                    strcpy(order.orderNo, orderNo);
                    strcpy(order.dueDate, dueDate);
                    order.qty = qty;
                    strcpy(order.productName, productName);
                    orderList[orderCounter] = order;
                    orderCounter++;
                } else {
                    printf("Failed to parse line: %s", line);
                }
            }
            ++lineNo;
        }
        fclose(file);
        return orderCounter;
    }

    int* printOrdersByPlant(Order_Plant orders[1000], int count, int totalDaysUsed) {//count = order total
        int* totalUsed = malloc(3 * sizeof(int));
        // int* totalDayUsed = malloc(3 * sizeof(int));//
        // if (totalUsed == NULL) {
        //     printf("Memory allocation failed.\n");
        //     return NULL;
        // }
        for(i = 0; i<3;i++){
            totalUsed[i] = 0;
        }
        for (i = 0; i < 3; i++) {  // Assuming 3 plants
            printf("\nPlant %s (%d per day)\n", orders[i].plant.plantName, orders[i].plant.capacity);
            printf("%s to %s\n", orders[i].periodStart,orders[i].periodEnd);
            printf("Date \t\t ProductName \t OrderNumber \t Quantity \t Due Date\n");
            int daysCounter,totalDaysCounter = 0;
            char lastDay[20];
            for (j = 0; j < count; j++) {
                for(daysCounter = 1; daysCounter<=orders[j].dayUsed;daysCounter++){
                    if (strcmp(orders[j].plant.plantName, orders[i].plant.plantName) == 0) {
                        int printQty;
                        //if not the last day print the plant capacity
                        // if(daysCounter==orders[j].dayUsed)printQty = orders[j].plant.capacityUsed;
                        // else printQty = orders[j].plant.capacity;
                        if(daysCounter==orders[j].dayUsed){
                            printQty = orders[j].plant.capacityUsed;
                            totalUsed[i]+=printQty;
                            // totalDayUsed[i] += orders[j].dayUsed;   //
                        }
                        else{ 
                            printQty = orders[j].plant.capacity;
                            totalUsed[i]+=printQty;
                            // totalDayUsed[i] += orders[j].dayUsed;   //
                        }
                        strcpy(lastDay, addMoreDays(orders[j].productStartDate, daysCounter));//store last day
                        printf("%s \t| %s \t| %s \t| %d \t\t| %s\n",
                        addMoreDays(orders[j].productStartDate, daysCounter),
                        orders[j].Order.productName,
                        orders[j].Order.orderNo, 
                        printQty, 
                        orders[j].Order.dueDate);
                        totalDaysCounter++;
                    }
                }
            }
            //print NA after all order printed
            for (j = 1; j <= totalDaysUsed-totalDaysCounter; j++) {
                printf("%s \t|\tNA\n",
                    addMoreDays(lastDay, j));
            }
        }
        return totalUsed;
    }

    void p3CreateFile(char reportName[20], int orderCounter, Order_Plant orderPlantList[], int rejectCounter, Order_Plant rejectList[],  char algoType[10], char start_date[20], char end_date[20], int totalUsed[3]){
        // printf("p3CreateFile | %s %s %s %s order: %d reject: %d\n", reportName, algoType, start_date, end_date, orderCounter, rejectCounter);
        // printf("ORDER NUMBER\t END\t DAYS\t\t QUANTITY\t PLANT\n");    //START\t
        for(i=0; i<orderCounter; i++){
            char startDate_order[20];
            char endDate_order[20];
            if(compare2Date(orderPlantList[i].productStartDate, start_date)){
                strcpy(startDate_order, start_date);
            }else {
                strcpy(startDate_order, orderPlantList[i].productStartDate);
            }
            if(compare2Date(orderPlantList[i].currentDate, start_date)){
                strcpy(endDate_order, start_date);
            }else{
                strcpy(endDate_order, orderPlantList[i].currentDate);
            } 
        }
        int totalQty_canProduce[] = {0, 0, 0};
        double utilization[] = {0, 0, 0};
        int totalDaysUsed[] = {0, 0, 0};
        int periodTotalDays = calDays(start_date,end_date);
        for(i=0;i<3;i++){
            //printf("Plant %s (%d per day)\n", orderPlantList[i].plant.plantName, orderPlantList[i].plant.capacity);
            for (j = 0; j < orderCounter; j++) {
                if(strcmp(orderPlantList[j].plant.plantName, orderPlantList[i].plant.plantName) == 0){
                    // totalQty_canProduce[i] += orderPlantList[j].plant.capacity;
                    totalDaysUsed[i] += orderPlantList[j].dayUsed;
                }
            }   
        }
        for(i=0;i<3;i++){
            totalQty_canProduce[i] = orderPlantList[i].plant.capacity * periodTotalDays;
            utilization[i] =  (double)totalUsed[i] / totalQty_canProduce[i] * 100.0;
        }

        FILE *file = fopen(reportName, "w");
        if (file == NULL) {
            printf("Error opening file");
            return;
        }
        fprintf(file, "***PLS Schedule Analysis Report***\n\n");
        fprintf(file, "Algorithm used: %s\n\n", algoType);
        fprintf(file, "There are %d Orders ACCEPTED.\tDetails are as follows:\n\n", orderCounter/3);
        fprintf(file, "ORDER NUMBER\tSTART\t\tEND\t\tDAYS\tQUANTITY\tPLANT\n");
        fprintf(file, "====================================================================================\n");

        for(i=0; i<orderCounter; i++){
            char startDate_order[20];
            char endDate_order[20];
            if(compare2Date(orderPlantList[i].productStartDate, start_date)){
                strcpy(startDate_order, start_date);
            }else{
                strcpy(startDate_order, orderPlantList[i].productStartDate);
            }
            if(compare2Date(orderPlantList[i].currentDate, start_date)){
                strcpy(endDate_order, start_date);
            }else{
                strcpy(endDate_order, orderPlantList[i].currentDate);
            }
            // printf("%s Plant %s current date: %s date used: %d qty: %d start date: %s\n",
            //         orderPlantList[i].Order.orderNo, orderPlantList[i].plant.plantName, endDate_order, orderPlantList[i].dayUsed, orderPlantList[i].plant.capacityUsed, startDate_order);      //orderPlantList[i].productStartDate
            if(orderPlantList[i].dayUsed!=0){
                fprintf(file, "%s\t\t%s\t%s\t%d\t%d\t\tPLANT_%s\n",
                    orderPlantList[i].Order.orderNo,  
                    startDate_order,
                    endDate_order,
                    orderPlantList[i].dayUsed,
                    orderPlantList[i].plant.capacityUsed,
                    orderPlantList[i].plant.plantName
                );
            }
        }
        fprintf(file, "\n\t\t\t- End -\n\n");
        fprintf(file, "====================================================================================\n\n");
        fprintf(file, "There are %d Orders REJECTED.\tDetails are as follows:\n\n", rejectCounter/3);
        fprintf(file, "ORDER NUMBER\tPRODUCT NAME\tDue Date\tQUANTITY\n");
        fprintf(file, "====================================================================================\n");
        for(i=0; i<rejectCounter/3; i++){
            char startDate_order[20];
            char endDate_order[20];
            // printf("%s Product_%s Due %s %d\n",
            //         rejectList[i].Order.orderNo, rejectList[i].plant.plantName, endDate_order, rejectList[i].dayUsed, rejectList[i].plant.capacityUsed, startDate_order);      //rejectList[i].productStartDate
                fprintf(file, "%s\t\t%s\t%s\t%d\n",
                    rejectList[i*3].Order.orderNo,  
                    rejectList[i*3].Order.productName,
                    rejectList[i*3].Order.dueDate,
                    rejectList[i*3].Order.qty
                );
        }
        fprintf(file, "\n\t\t\t- End -\n\n");
        fprintf(file, "====================================================================================\n");
        fprintf(file, "***PERFORMANCE\n\n");
        int overallDaysUse = 0, overallProduced = 0;
        for(i=0;i<3;i++){
            fprintf(file, "Plant_%s:\n", orderPlantList[i].plant.plantName);
            fprintf(file, "\t\tNumber of days in use:\t\t\t\t\t");
            fprintf(file, "%d days\n", totalDaysUsed[i]);
            fprintf(file, "\t\tNumber of products produced:\t\t\t");
            fprintf(file, "%d (in total)\n", totalUsed[i]);
            fprintf(file, "\t\tUtilization of the plant:\t\t\t\t");
            fprintf(file, "%.2f%%\n", utilization[i]);
            overallDaysUse+=totalUsed[i];
            overallProduced+=totalQty_canProduce[i];
        }
        double overall =  (double)overallDaysUse / overallProduced * 100.0;
        fprintf(file, "\nOverall of utilization:\t\t\t\t");
        fprintf(file, "\t\t\t%.2f%%\n", overall);
        fclose(file);
    }

int prRanking(char* productName) {
    if (strcmp(productName, "Product_A") == 0) return 1200;
    else if (strcmp(productName, "Product_B") == 0) return 1100;
    else if (strcmp(productName, "Product_C") == 0) return 1000;
    else if (strcmp(productName, "Product_D") == 0) return 900;
    else if (strcmp(productName, "Product_E") == 0) return 800;
    else if (strcmp(productName, "Product_F") == 0) return 700;
    else if (strcmp(productName, "Product_G") == 0) return 600;
    else if (strcmp(productName, "Product_H") == 0) return 500;
    else if (strcmp(productName, "Product_I") == 0) return 400;
    return 300; // Default ranking if not matched
}

void sortingSJF(Order *orderList, int n) {
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - i - 1; j++) {
            if (orderList[j].qty > orderList[j + 1].qty) {
                Order tempOrder = orderList[j];
                orderList[j] = orderList[j + 1];
                orderList[j + 1] = tempOrder;
            }
        }
    }
}

void sortingPR(Order *orderList, int n) {
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - i - 1; j++) {
            if (prRanking(orderList[j].productName) < prRanking(orderList[j + 1].productName)) {
                Order tempOrder = orderList[j];
                orderList[j] = orderList[j + 1];
                orderList[j + 1] = tempOrder;
            }
        }
    }
}

void sortingDDF(Order *orderList, int n) {
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - i - 1; j++) {
            if (!compare2Date(orderList[j].dueDate, orderList[j + 1].dueDate)) {
                Order tempOrder = orderList[j];
                orderList[j] = orderList[j + 1];
                orderList[j + 1] = tempOrder;
            }
        }
    }
}

void inputLog( char data[20]){
    FILE *file = fopen("record.txt", "a");
    if (file == NULL) {
        printf("Error opening file");
        return;
    }
    fputs(data, file);
    fputs("\n", file);
    fclose(file);
}

    // ======================================== Different Module Function ========================================
    // child handle Scheduling Process
    void childSchedulingProcess(){
        while(1){
            n = read(pToC1[0],buf_c1,80);
            buf_c1[n] = 0;
            if(strcmp(buf_c1, "exitPLS")==0) {
                write(c1ToC2[1], buf_c1, 80);
                break;
            }else{
                orderCounter = c1ReadFile(buf_c1, orderList, start_date, end_date, algoType, reportName);
                int x;
                if(strcmp("SJF", algoType) == 0){
                    sortingSJF(orderList,orderCounter);
                } else if(strcmp("PR", algoType) == 0){
                    sortingPR(orderList,orderCounter);
                } else if(strcmp("DDF", algoType) == 0){ //dueDate first
                    sortingDDF(orderList,orderCounter);
                }
                orderPlantCounter = 0;
                rejectCounter = 0;
                for(i = 0; i<3;i++){
                    strcpy(plantDays[i], addMoreDays(start_date,-1));
                }
                for(x=0; x<orderCounter; x++){
                    previous_Max = 100;//INT8_MAX;//define and reset max no //may hardcode to 100 or more or total peiod
                    algo(x);//after run algo() the best workload will save to storeBest[i]
                }
                int intCount =0;    //debug
                for(i = 0; i<orderPlantCounter ;i++){
                    int x;
                    for(x = 0;x<orderPlant[i].dayUsed;x++){
                        intCount++;
                    }
                }
                int size = sizeof(orderPlant)/ sizeof(orderPlant[0]);
                write(c1ToC2[1], buf_c1, 20);
                write(c1ToC2[1], end_date, 20);
                write(c1ToC2[1], start_date, 20);
                write(c1ToC2[1], algoType, 20);
                write(c1ToC2[1], reportName, 20);
                write(c1ToC2[1], &orderPlantCounter, sizeof(orderPlantCounter));
                for (i = 0; i < orderPlantCounter; i++)
                {
                    write(c1ToC2[1], &orderPlant[i], sizeof(Order_Plant));
                }     
                //rejectCounter
                write(c1ToC2[1], &rejectCounter, sizeof(rejectCounter));
                for (i = 0; i < rejectCounter; i++) {
                    write(c1ToC2[1], &rejectedOrderPlant[i], sizeof(Order_Plant));
                }
                strcpy(buf_c1, "c1ToC2");
                write(c1ToC2[1], buf_c1, 80);
            }
        }
    }

    // child handle output Process
    void childOutputProcess(){
        while(1){
            n = read(c1ToC2[0], buf_c2, 20);
            buf_c2[n] = 0;
            if(strcmp(buf_c2, "exitPLS")==0){
                write(c2ToC3[1], buf_c2, 80);
                break;
            }else{
                int localOrderPlantCounter;
                int localRejectCounter;
                char localStartDate[20];
                n = read(c1ToC2[0], end_date, 20);
                end_date[n] = 0;
                n = read(c1ToC2[0], localStartDate, 20);
                localStartDate[n] = 0;
                n = read(c1ToC2[0], algoType, 20);
                algoType[n] = 0;
                n = read(c1ToC2[0], reportName, 20);
                reportName[n] = 0;
                read(c1ToC2[0], &localOrderPlantCounter, sizeof(localOrderPlantCounter));
                int totalDaysUsed = calDays(localStartDate,end_date);
                Order_Plant *receivedOrders = malloc(localOrderPlantCounter * sizeof(Order_Plant));
                for (i = 0; i < localOrderPlantCounter; i++) {  //localOrderPlantCounter
                    n = read(c1ToC2[0], &receivedOrders[i], sizeof(Order_Plant));
                    if(n>0){
                        
                     }else{
                        free(receivedOrders);
                    }
                }
                //create an element to store the plant utilization
                int* totalUsed = malloc(3 * sizeof(int));
                totalUsed =  printOrdersByPlant(receivedOrders, localOrderPlantCounter, totalDaysUsed);
                read(c1ToC2[0], &localRejectCounter, sizeof(localRejectCounter));
                Order_Plant *receivedRejectedOrders = malloc(localRejectCounter * sizeof(Order_Plant));
                for (i = 0; i < localRejectCounter; i++) {    //localRejectCounter
                    n = read(c1ToC2[0], &receivedRejectedOrders[i], sizeof(Order_Plant));
                    if(n>0){ 
                        // printf("Received Reject Order %s from Plant %s\n",
                        // receivedRejectedOrders[i].Order.orderNo, receivedRejectedOrders[i].plant.plantName);      
                    }else{
                        free(receivedRejectedOrders);
                    } 
                }
                n = read(c1ToC2[0], buf_c2, 80);
                buf_c2[n] = 0;
                if(strcmp(buf_c2, "c1ToC2") == 0){
                    write(c2ToC3[1],buf_c2,n);
                    write(c2ToC3[1], localStartDate, 20);
                    write(c2ToC3[1], end_date, 20);
                    write(c2ToC3[1], algoType, 20);
                    write(c2ToC3[1], reportName, 20);
                    write(c2ToC3[1], &localOrderPlantCounter, sizeof(localOrderPlantCounter));
                    write(c2ToC3[1], &localRejectCounter, sizeof(localRejectCounter));
                    for ( i = 0; i < localOrderPlantCounter; i++){
                        write(c2ToC3[1], &receivedOrders[i], sizeof(Order_Plant));
                    }
                    for ( i = 0; i < localRejectCounter; i++){
                        write(c2ToC3[1], &receivedRejectedOrders[i], sizeof(Order_Plant));
                    }
                    for ( i = 0; i < 3; i++) {
                        if (write(c2ToC3[1], &totalUsed[i], sizeof(int)) == -1) {
                            printf("Failed to write to pipe\n");
                            free(totalUsed);     
                        }
                    }
                    strcpy(buf_c2, "c2ToC3");
                    write(c2ToC3[1],buf_c2,n); 
                }
            }
        }
    }
    void grandchildAnalyzerProcess(){
        while(1){
            n = read(c2ToC3[0],buf_c3,80);
            buf_c3[n] = 0;
            if(strcmp(buf_c3, "exitPLS")==0){
                write(c3ToP[1], buf_c3, 80);
                break;
            }else{
                int localOrderPlantCounter;
                int localRejectCounter;
                char localStartDate[20];
                n = read(c2ToC3[0], localStartDate, 20);
                localStartDate[n] = 0;
                n = read(c2ToC3[0], end_date, 20);
                end_date[n] = 0;
                n = read(c2ToC3[0], algoType, 20);
                algoType[n] = 0;
                n = read(c2ToC3[0], reportName, 20);
                reportName[n] = 0;
                read(c2ToC3[0], &localOrderPlantCounter, sizeof(localOrderPlantCounter));
                read(c2ToC3[0], &localRejectCounter, sizeof(localRejectCounter));
                Order_Plant *receivedOrders2 = malloc(localOrderPlantCounter * sizeof(Order_Plant));
                for (i = 0; i < localOrderPlantCounter; i++) { 
                    n = read(c2ToC3[0], &receivedOrders2[i], sizeof(Order_Plant));
                    if(n>0){
                        //Debug
                        // printf("Received Order %s from Plant %s | due date: %s current date: %s date used: %d\n",
                        // receivedOrders2[i].Order.orderNo, receivedOrders2[i].plant.plantName, receivedOrders2[i].Order.dueDate, receivedOrders2[i].currentDate, receivedOrders2[i].dayUsed);      
                    }else{
                        free(receivedOrders2);
                    }
                }
                Order_Plant *receivedRejectedOrders2 = malloc(localRejectCounter * sizeof(Order_Plant));
                for (i = 0; i < localRejectCounter; i++) {    //localRejectCounter
                    n = read(c2ToC3[0], &receivedRejectedOrders2[i], sizeof(Order_Plant));
                    if(n>0){
                        // printf("Received Reject Order %s from Plant %s\n",
                        // receivedRejectedOrders2[i].Order.orderNo, receivedRejectedOrders2[i].plant.plantName);      
                    }else{
                        free(receivedRejectedOrders2);
                    }
                    
                }
                
                int* totalUsed = malloc(3 * sizeof(int));
                
                for (i = 0; i < 3; i++) {     
                    if (read(c2ToC3[0], &totalUsed[i], sizeof(int)) == -1) {
                        printf("Failed to read from pipe\n");
                        free(totalUsed);
                    }
                }

                p3CreateFile(reportName, localOrderPlantCounter, receivedOrders2, localRejectCounter, receivedRejectedOrders2, algoType, localStartDate, end_date, totalUsed);


                n = read(c2ToC3[0],buf_c3,80);
                buf_c3[n] = 0;
                if(strcmp(buf_c3, "c2ToC3") == 0){
                    write(c3ToP[1],buf_c3,n);
                }
            }
            
        }
    }

    void parentProcess(){
        printf("    ~~~WELCOME TO PLS~~\n\n");
        while(1){
            clearTokens(tokens, 20);
            printf("Please enter:\n");        
            n = read(STDIN_FILENO,buf_p,80); 
            if (n <= 0) break;
            buf_p[--n] = 0;
            inputLog(buf_p);

            tokenCount = 0;
            char *token;
            token = strtok(buf_p, " ");

            while(token != NULL){
                // printf( "%s\n", token);
                tokens[tokenCount] = strdup(token);
                tokenCount++;
                token = strtok(NULL, " ");
            }

            if(strcmp(tokens[0], "addPERIOD")==0){
                addPERIOD(start_date, end_date, tokens);
                orderCounter = 0;
                period = calDays(start_date, end_date);
            }else if(strcmp(tokens[0], "addORDER")==0 && strcmp(start_date, "") !=0){
                
                Order tempOrder = addORDER(tokens);
                
                if (compare2Date(tempOrder.dueDate, addMoreDays(end_date,1)) && compare2Date(addMoreDays(start_date,-1),tempOrder.dueDate)) {
                    orderList[orderCounter] = tempOrder;
                    orderCounter++;//
                }else{
                    printf("Please enter the correct dates!\n");
                    continue;
                }
                
            }else if(strcmp(tokens[0], "addBATCH")==0 && strcmp(start_date, "") !=0){
                orderCounter = addBATCH(tokens[1], orderList, orderCounter, end_date);


            }else if(strcmp(tokens[0], "runPLS")==0 && strcmp(start_date, "") !=0){
                strcpy(algoType, tokens[1]);
                strcpy(reportName, tokens[5]);
                p2C1CreateFile(orderList, orderCounter, algoType, start_date, end_date, reportName);
                strcpy(buf_p, "forSchedulingModule.txt");
                write(pToC1[1],buf_p,n); 
                n = read(c3ToP[0], buf_p, 300);
                buf_p[n] = 0;
            }else if(strcmp(tokens[0], "exitPLS")==0){
                write(pToC1[1],buf_p,n);
                n = read(c3ToP[0], buf_p, 300);
                buf_p[n] = 0;
                printf("Bye-bye!\n");
                exit(0);
                break;
            }

            if(strcmp(start_date, "")==0){
                printf("Please input the start and end dates!\n");
                continue;
            }


        }
    }

    //Scheduling=>output=>parent=>Analyzer
    int main() {
        init();
        if (pipe(pToC1) < 0) {
            printf("Pipe creation error\n");
            exit(1);
        }
        if (pipe(c1ToC2) < 0) {
            printf("Pipe creation error\n");
            exit(1);
        }
        if (pipe(c2ToC3) < 0) {
            printf("Pipe creation error\n");
            exit(1);
        }
        if (pipe(c3ToP) < 0) {
            printf("Pipe creation error\n");
            exit(1);
        }

        pid = fork();
        if (pid < 0) {
            printf("Fork failed\n");
            exit(1);
        } else if (pid == 0) { 
            pid = fork();
            if(pid <0){
                printf("Fork failed\n");
                exit(1);
            }else if(pid == 0){     
                pid = fork();
                if(pid <0){          
                    printf("Fork failed\n");
                    exit(1);
                }else if(pid == 0){     //grandchild (3)    Analyzer module
                    close(pToC1[0]); close(pToC1[1]); close(c1ToC2[1]); close(c1ToC2[0]); close(c2ToC3[1]); close(c3ToP[0]);
                    grandchildAnalyzerProcess();  
                    close(c2ToC3[0]); close(c3ToP[1]);
                }else{  // grandchild (2)   Output module
                    close(pToC1[0]); close(pToC1[1]); close(c1ToC2[1]); close(c2ToC3[0]);
                    childOutputProcess();
                    close(c1ToC2[0]); close(c2ToC3[1]);
                }
            }else{  //child (1)   Scheduling module
                close(pToC1[1]); close(c1ToC2[0]);// close child out
                childSchedulingProcess();
                close(pToC1[0]); close(c1ToC2[1]);
            }
        }else{      //parent        Input module
            close(pToC1[0]);
            parentProcess();
            close(pToC1[1]);
            wait(NULL);
        }
        return 0;
    }